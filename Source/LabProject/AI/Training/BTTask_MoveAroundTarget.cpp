#include "AI/Training/BTTask_MoveAroundTarget.h"
#include "AI/Training/TrainingBotMovement.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "Character/EnemyBase.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Navigation/PathFollowingComponent.h"

namespace
{
	enum class EMoveAroundTargetPhase : uint8
	{
		MoveAround,
		AttackApproach,
		AttackWindow
	};

	struct FMoveAroundTargetMemory
	{
		FVector Destination = FVector::ZeroVector;
		float ElapsedTime = 0.0f;
		int32 CompletedMoveCount = 0;
		int32 MovesBeforeAttack = 1;
		float NextComboAttackRequestTime = 0.0f;
		EMoveAroundTargetPhase Phase = EMoveAroundTargetPhase::MoveAround;
		uint8 bAttackAbilityObservedActive : 1;
		uint8 bChangedFacingMode : 1;
		uint8 bPreviousOrientRotationToMovement : 1;
		uint8 bPreviousUseControllerDesiredRotation : 1;
		uint8 bPreviousUseControllerRotationYaw : 1;
	};
}

UBTTask_MoveAroundTarget::UBTTask_MoveAroundTarget()
{
	NodeName = TEXT("Pd Move Around Target");
	bNotifyTick = true;
	bNotifyTaskFinished = true;

	BlackboardKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(ThisClass, BlackboardKey), AActor::StaticClass());
}

uint16 UBTTask_MoveAroundTarget::GetInstanceMemorySize() const
{
	return sizeof(FMoveAroundTargetMemory);
}

FString UBTTask_MoveAroundTarget::GetStaticDescription() const
{
	return FString::Printf(
		TEXT("%s: around %s\nDistance %.0f-%.0f, side %d..%d * %.0f, accept %.0f, moves before attack %d-%d, approach=%s, attack=%s, max combo wait %.1fs, keep facing=%s"),
		*Super::GetStaticDescription(),
		*BlackboardKey.SelectedKeyName.ToString(),
		MinDistanceFromTarget,
		MaxDistanceFromTarget,
		MinSideStepMultiplier,
		MaxSideStepMultiplier,
		SideStepDistance,
		AcceptanceRadius,
		MinMovesBeforeAttack,
		MaxMovesBeforeAttack,
		bApproachTargetBeforeAttack ? TEXT("true") : TEXT("false"),
		bAttackAfterMoves ? TEXT("true") : TEXT("false"),
		MaxComboAttackWaitTime,
		bKeepFacingTargetAfterMove ? TEXT("true") : TEXT("false"));
}

EBTNodeResult::Type UBTTask_MoveAroundTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FMoveAroundTargetMemory* Memory = reinterpret_cast<FMoveAroundTargetMemory*>(NodeMemory);
	*Memory = FMoveAroundTargetMemory();

	AAIController* AIController = OwnerComp.GetAIOwner();
	APawn* Pawn = AIController ? AIController->GetPawn() : nullptr;
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	AActor* TargetActor = Blackboard ? Cast<AActor>(Blackboard->GetValueAsObject(BlackboardKey.SelectedKeyName)) : nullptr;

	if (!AIController || !Pawn || !TargetActor)
	{
		return EBTNodeResult::Failed;
	}

	Memory->MovesBeforeAttack = FMath::RandRange(
		FMath::Max(1, FMath::Min(MinMovesBeforeAttack, MaxMovesBeforeAttack)),
		FMath::Max(1, FMath::Max(MinMovesBeforeAttack, MaxMovesBeforeAttack)));
	ApplyFacingMode(Pawn, NodeMemory);
	UpdateFacing(AIController, Pawn, TargetActor, 0.0f);
	if (const AEnemyBase* FrozenEnemy = Cast<AEnemyBase>(Pawn); !FrozenEnemy || !FrozenEnemy->IsStatusFrozen())
	{
		AIController->SetFocus(TargetActor, EAIFocusPriority::Gameplay);
	}

	return RequestNextMove(OwnerComp, NodeMemory, Pawn, TargetActor);
}

void UBTTask_MoveAroundTarget::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	FMoveAroundTargetMemory* Memory = reinterpret_cast<FMoveAroundTargetMemory*>(NodeMemory);

	AAIController* AIController = OwnerComp.GetAIOwner();
	APawn* Pawn = AIController ? AIController->GetPawn() : nullptr;
	if (!AIController || !Pawn)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	AActor* TargetActor = Blackboard ? Cast<AActor>(Blackboard->GetValueAsObject(BlackboardKey.SelectedKeyName)) : nullptr;
	UpdateFacing(AIController, Pawn, TargetActor, DeltaSeconds);

	Memory->ElapsedTime += DeltaSeconds;
	if (Memory->Phase == EMoveAroundTargetPhase::AttackWindow)
	{
		const EBTNodeResult::Type AttackWindowResult = TickAttackWindow(OwnerComp, NodeMemory, Pawn, TargetActor);
		if (AttackWindowResult != EBTNodeResult::InProgress)
		{
			FinishLatentTask(OwnerComp, AttackWindowResult);
		}
		return;
	}

	if (Memory->Phase == EMoveAroundTargetPhase::AttackApproach)
	{
		const EBTNodeResult::Type ApproachResult = TryFinishAttackApproach(OwnerComp, NodeMemory, Pawn, TargetActor);
		if (ApproachResult != EBTNodeResult::InProgress)
		{
			FinishLatentTask(OwnerComp, ApproachResult);
			return;
		}

		if (MaxAttackApproachTime > 0.0f && Memory->ElapsedTime >= MaxAttackApproachTime)
		{
			AIController->StopMovement();
			RestoreMovementSettings(Pawn, NodeMemory, !bTreatAttackApproachTimeoutAsSuccess || !bKeepFacingTargetAfterMove);
			FinishLatentTask(OwnerComp, bTreatAttackApproachTimeoutAsSuccess ? EBTNodeResult::Succeeded : EBTNodeResult::Failed);
			return;
		}

		return;
	}

	const float DistanceToDestination = FVector::Dist2D(Pawn->GetActorLocation(), Memory->Destination);
	if (DistanceToDestination <= AcceptanceRadius + FinishDistanceTolerance)
	{
		UpdateFacing(AIController, Pawn, TargetActor, 0.0f);
		const EBTNodeResult::Type Result = CompleteOneMoveAndMaybeContinue(OwnerComp, NodeMemory, Pawn, TargetActor);
		if (Result != EBTNodeResult::InProgress)
		{
			FinishLatentTask(OwnerComp, Result);
		}
		return;
	}

	if (MaxMoveTime > 0.0f && Memory->ElapsedTime >= MaxMoveTime)
	{
		AIController->StopMovement();
		if (bTreatTimeoutAsSuccess)
		{
			const EBTNodeResult::Type Result = RequestAttackApproach(OwnerComp, NodeMemory, Pawn, TargetActor);
			if (Result != EBTNodeResult::InProgress)
			{
				FinishLatentTask(OwnerComp, Result);
			}
			return;
		}
		RestoreMovementSettings(Pawn, NodeMemory, !bTreatTimeoutAsSuccess || !bKeepFacingTargetAfterMove);
		FinishLatentTask(OwnerComp, bTreatTimeoutAsSuccess ? EBTNodeResult::Succeeded : EBTNodeResult::Failed);
		return;
	}

	const UPathFollowingComponent* PathFollowingComponent = AIController->GetPathFollowingComponent();
	if (PathFollowingComponent && PathFollowingComponent->GetStatus() == EPathFollowingStatus::Idle)
	{
		const bool bSucceeded = DistanceToDestination <= AcceptanceRadius + FinishDistanceTolerance;
		if (bSucceeded)
		{
			UpdateFacing(AIController, Pawn, TargetActor, 0.0f);
		}
		RestoreMovementSettings(Pawn, NodeMemory, !bSucceeded || !bKeepFacingTargetAfterMove);
		FinishLatentTask(OwnerComp, bSucceeded ? EBTNodeResult::Succeeded : EBTNodeResult::Failed);
	}
}

void UBTTask_MoveAroundTarget::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	APawn* Pawn = AIController ? AIController->GetPawn() : nullptr;
	const bool bRestoreFacing = TaskResult != EBTNodeResult::Succeeded || !bKeepFacingTargetAfterMove;
	RestoreMovementSettings(Pawn, NodeMemory, bRestoreFacing);

	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);
}

EBTNodeResult::Type UBTTask_MoveAroundTarget::RequestNextMove(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, APawn* Pawn, AActor* TargetActor)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController || !Pawn || !TargetActor)
	{
		RestoreMovementSettings(Pawn, NodeMemory, true);
		return EBTNodeResult::Failed;
	}

	FMoveAroundTargetMemory* Memory = reinterpret_cast<FMoveAroundTargetMemory*>(NodeMemory);
	Memory->ElapsedTime = 0.0f;
	Memory->Phase = EMoveAroundTargetPhase::MoveAround;

	FVector Destination = FVector::ZeroVector;
	if (!BuildMoveDestination(Pawn, TargetActor, Destination))
	{
		RestoreMovementSettings(Pawn, NodeMemory, true);
		return EBTNodeResult::Failed;
	}

	Memory->Destination = Destination;
	UpdateFacing(AIController, Pawn, TargetActor, 0.0f);
	if (const AEnemyBase* FrozenEnemy = Cast<AEnemyBase>(Pawn); !FrozenEnemy || !FrozenEnemy->IsStatusFrozen())
	{
		AIController->SetFocus(TargetActor, EAIFocusPriority::Gameplay);
	}

	const EPathFollowingRequestResult::Type MoveResult = TrainingBotMovement::MoveToLocation(*AIController, Destination, AcceptanceRadius, bStopOnOverlap);

	if (bDrawDebug)
	{
		DrawDebugSphere(Pawn->GetWorld(), Destination, 35.0f, 16, FColor::Cyan, false, MaxMoveTime > 0.0f ? MaxMoveTime : 1.5f);
		DrawDebugLine(Pawn->GetWorld(), Pawn->GetActorLocation(), Destination, FColor::Cyan, false, MaxMoveTime > 0.0f ? MaxMoveTime : 1.5f, 0, 2.0f);
	}

	if (MoveResult == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		return CompleteOneMoveAndMaybeContinue(OwnerComp, NodeMemory, Pawn, TargetActor);
	}

	if (MoveResult == EPathFollowingRequestResult::Failed)
	{
		RestoreMovementSettings(Pawn, NodeMemory, true);
		return EBTNodeResult::Failed;
	}

	return EBTNodeResult::InProgress;
}

EBTNodeResult::Type UBTTask_MoveAroundTarget::CompleteOneMoveAndMaybeContinue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, APawn* Pawn, AActor* TargetActor)
{
	FMoveAroundTargetMemory* Memory = reinterpret_cast<FMoveAroundTargetMemory*>(NodeMemory);
	++Memory->CompletedMoveCount;

	if (Memory->CompletedMoveCount >= Memory->MovesBeforeAttack)
	{
		return RequestAttackApproach(OwnerComp, NodeMemory, Pawn, TargetActor);
	}

	return RequestNextMove(OwnerComp, NodeMemory, Pawn, TargetActor);
}

EBTNodeResult::Type UBTTask_MoveAroundTarget::RequestAttackApproach(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, APawn* Pawn, AActor* TargetActor)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	AEnemyBase* Enemy = Cast<AEnemyBase>(Pawn);
	if (!AIController || !Enemy || !Enemy->IsActorValidAttackTarget(TargetActor))
	{
		RestoreMovementSettings(Pawn, NodeMemory, true);
		return EBTNodeResult::Failed;
	}

	if (!bAttackAfterMoves)
	{
		RestoreMovementSettings(Pawn, NodeMemory, !bKeepFacingTargetAfterMove);
		return EBTNodeResult::Succeeded;
	}

	UpdateFacing(AIController, Pawn, TargetActor, 0.0f);
	const float AttackDistance = Enemy->GetAttackDistanceToActor(TargetActor);
	const float AttackRange = Enemy->GetAttackStartDistance();
	if (!bApproachTargetBeforeAttack || AttackDistance <= AttackRange)
	{
		return StartAttackWindow(OwnerComp, NodeMemory, Pawn, TargetActor);
	}

	FMoveAroundTargetMemory* Memory = reinterpret_cast<FMoveAroundTargetMemory*>(NodeMemory);
	Memory->Phase = EMoveAroundTargetPhase::AttackApproach;
	Memory->ElapsedTime = 0.0f;
	if (!Enemy->IsStatusFrozen())
	{
		AIController->SetFocus(TargetActor, EAIFocusPriority::Gameplay);
	}

	const EPathFollowingRequestResult::Type MoveResult = AIController->MoveToActor(
		TargetActor,
		AttackRange,
		true,
		true,
		true,
		nullptr,
		true);

	if (MoveResult == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		return TryFinishAttackApproach(OwnerComp, NodeMemory, Pawn, TargetActor);
	}

	if (MoveResult == EPathFollowingRequestResult::Failed)
	{
		RestoreMovementSettings(Pawn, NodeMemory, true);
		return EBTNodeResult::Failed;
	}

	return EBTNodeResult::InProgress;
}

EBTNodeResult::Type UBTTask_MoveAroundTarget::TryFinishAttackApproach(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, APawn* Pawn, AActor* TargetActor)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	AEnemyBase* Enemy = Cast<AEnemyBase>(Pawn);
	if (!AIController || !Enemy || !Enemy->IsActorValidAttackTarget(TargetActor))
	{
		RestoreMovementSettings(Pawn, NodeMemory, true);
		return EBTNodeResult::Failed;
	}

	const float AttackDistance = Enemy->GetAttackDistanceToActor(TargetActor);
	const float AttackRange = Enemy->GetAttackStartDistance();
	if (AttackDistance > AttackRange)
	{
		return EBTNodeResult::InProgress;
	}

	return StartAttackWindow(OwnerComp, NodeMemory, Pawn, TargetActor);
}

EBTNodeResult::Type UBTTask_MoveAroundTarget::StartAttackWindow(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, APawn* Pawn, AActor* TargetActor)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	AEnemyBase* Enemy = Cast<AEnemyBase>(Pawn);
	if (!AIController || !Enemy || !Enemy->IsActorValidAttackTarget(TargetActor))
	{
		RestoreMovementSettings(Pawn, NodeMemory, true);
		return EBTNodeResult::Failed;
	}

	FMoveAroundTargetMemory* Memory = reinterpret_cast<FMoveAroundTargetMemory*>(NodeMemory);
	Memory->Phase = EMoveAroundTargetPhase::AttackWindow;
	Memory->ElapsedTime = 0.0f;
	Memory->bAttackAbilityObservedActive = false;
	Memory->NextComboAttackRequestTime = FMath::Max(ComboAttackRequestInterval, 0.05f);

	TryAttackAfterMoves(AIController, Enemy, TargetActor);
	return EBTNodeResult::InProgress;
}

EBTNodeResult::Type UBTTask_MoveAroundTarget::TickAttackWindow(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, APawn* Pawn, AActor* TargetActor)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	AEnemyBase* Enemy = Cast<AEnemyBase>(Pawn);
	if (!AIController || !Enemy || !Enemy->IsActorValidAttackTarget(TargetActor))
	{
		RestoreMovementSettings(Pawn, NodeMemory, true);
		return EBTNodeResult::Failed;
	}

	FMoveAroundTargetMemory* Memory = reinterpret_cast<FMoveAroundTargetMemory*>(NodeMemory);
	UpdateFacing(AIController, Enemy, TargetActor, OwnerComp.GetWorld() ? OwnerComp.GetWorld()->GetDeltaSeconds() : 0.0f);

	const bool bAttackAbilityActive = Enemy->IsAttackAbilityActive();
	if (bAttackAbilityActive)
	{
		Memory->bAttackAbilityObservedActive = true;
	}

	if (Memory->bAttackAbilityObservedActive && !bAttackAbilityActive)
	{
		RestoreMovementSettings(Pawn, NodeMemory, !bKeepFacingTargetAfterMove);
		return EBTNodeResult::Succeeded;
	}

	if (MaxComboAttackWaitTime > 0.0f && Memory->ElapsedTime >= MaxComboAttackWaitTime)
	{
		RestoreMovementSettings(Pawn, NodeMemory, !bKeepFacingTargetAfterMove);
		return EBTNodeResult::Succeeded;
	}

	if (Memory->ElapsedTime >= Memory->NextComboAttackRequestTime)
	{
		TryAttackAfterMoves(AIController, Enemy, TargetActor);
		Memory->NextComboAttackRequestTime += FMath::Max(ComboAttackRequestInterval, 0.05f);
	}

	return EBTNodeResult::InProgress;
}

bool UBTTask_MoveAroundTarget::TryAttackAfterMoves(AAIController* AIController, APawn* Pawn, AActor* TargetActor) const
{
	if (!bAttackAfterMoves)
	{
		return false;
	}

	AEnemyBase* Enemy = Cast<AEnemyBase>(Pawn);
	if (!AIController || !Enemy || !Enemy->IsActorValidAttackTarget(TargetActor))
	{
		return false;
	}

	if (bStopMovementBeforeAttack)
	{
		AIController->StopMovement();
	}

	UpdateFacing(AIController, Enemy, TargetActor, 0.0f);
	Enemy->SetAttackTarget(TargetActor);

	Enemy->Attack();
	return true;
}

bool UBTTask_MoveAroundTarget::BuildMoveDestination(APawn* Pawn, AActor* TargetActor, FVector& OutDestination) const
{
	if (!Pawn || !TargetActor)
	{
		return false;
	}

	OutDestination = TrainingBotMovement::BuildSideStepDestination(
		*Pawn,
		*TargetActor,
		MinDistanceFromTarget,
		MaxDistanceFromTarget,
		MinSideStepMultiplier,
		MaxSideStepMultiplier,
		SideStepDistance);
	if (bProjectDestinationToNavigation)
	{
		OutDestination = TrainingBotMovement::ProjectToNavigation(Pawn->GetWorld(), OutDestination, NavigationProjectionExtent);
	}
	return true;
}

void UBTTask_MoveAroundTarget::ApplyFacingMode(APawn* Pawn, uint8* NodeMemory) const
{
	if (!bFaceTargetWhileMoving || !Pawn)
	{
		return;
	}

	ACharacter* Character = Cast<ACharacter>(Pawn);
	UCharacterMovementComponent* MovementComponent = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Character || !MovementComponent)
	{
		return;
	}

	if (const AEnemyBase* Enemy = Cast<AEnemyBase>(Pawn); Enemy && Enemy->IsStatusFrozen())
	{
		return;
	}

	FMoveAroundTargetMemory* Memory = reinterpret_cast<FMoveAroundTargetMemory*>(NodeMemory);
	Memory->bPreviousOrientRotationToMovement = MovementComponent->bOrientRotationToMovement;
	Memory->bPreviousUseControllerDesiredRotation = MovementComponent->bUseControllerDesiredRotation;
	Memory->bPreviousUseControllerRotationYaw = Character->bUseControllerRotationYaw;
	Memory->bChangedFacingMode = true;

	MovementComponent->bOrientRotationToMovement = false;
	MovementComponent->bUseControllerDesiredRotation = true;
	Character->bUseControllerRotationYaw = true;
}

void UBTTask_MoveAroundTarget::UpdateFacing(AAIController* AIController, APawn* Pawn, AActor* TargetActor, float DeltaSeconds) const
{
	if (bFaceTargetWhileMoving && AIController && Pawn && TargetActor)
	{
		TrainingBotMovement::FaceTarget(*AIController, *Pawn, *TargetActor, DeltaSeconds, FaceTargetRotationInterpSpeed);
	}
}

void UBTTask_MoveAroundTarget::RestoreMovementSettings(APawn* Pawn, uint8* NodeMemory, bool bRestoreFacing) const
{
	FMoveAroundTargetMemory* Memory = reinterpret_cast<FMoveAroundTargetMemory*>(NodeMemory);
	if (!Pawn)
	{
		return;
	}

	ACharacter* Character = Cast<ACharacter>(Pawn);
	UCharacterMovementComponent* MovementComponent = Character ? Character->GetCharacterMovement() : nullptr;

	if (bRestoreFacing && Memory->bChangedFacingMode && Character && MovementComponent)
	{
		if (const AEnemyBase* Enemy = Cast<AEnemyBase>(Pawn); Enemy && Enemy->IsStatusFrozen())
		{
			MovementComponent->bOrientRotationToMovement = false;
			MovementComponent->bUseControllerDesiredRotation = false;
			Character->bUseControllerRotationYaw = false;
			Memory->bChangedFacingMode = false;

			return;
		}

		MovementComponent->bOrientRotationToMovement = Memory->bPreviousOrientRotationToMovement;
		MovementComponent->bUseControllerDesiredRotation = Memory->bPreviousUseControllerDesiredRotation;
		Character->bUseControllerRotationYaw = Memory->bPreviousUseControllerRotationYaw;
		Memory->bChangedFacingMode = false;
	}
}
