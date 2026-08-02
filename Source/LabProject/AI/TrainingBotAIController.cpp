#include "AI/TrainingBotAIController.h"

#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/EnemyBase.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogTrainingBotAIController, Log, All);

ATrainingBotAIController::ATrainingBotAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void ATrainingBotAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (AEnemyBase* Enemy = Cast<AEnemyBase>(InPawn))
	{
		Enemy->SetUseNearestPlayerWhenTargetUnset(true);
		Enemy->InitializeBehaviorTreeCombat();
	}

	if (!BehaviorTreeAsset)
	{
		UE_LOG(
			LogTrainingBotAIController,
			Error,
			TEXT("%s cannot start training bot AI because BehaviorTreeAsset is missing."),
			*GetPathName());
		return;
	}

	if (!BehaviorTreeAsset->BlackboardAsset)
	{
		UE_LOG(
			LogTrainingBotAIController,
			Error,
			TEXT("%s cannot start training bot AI because %s has no Blackboard asset."),
			*GetPathName(),
			*GetNameSafe(BehaviorTreeAsset));
		return;
	}

	UBlackboardComponent* LocalBlackboard = nullptr;
	if (!UseBlackboard(BehaviorTreeAsset->BlackboardAsset, LocalBlackboard))
	{
		UE_LOG(
			LogTrainingBotAIController,
			Error,
			TEXT("%s failed to initialize Blackboard %s for training bot %s."),
			*GetPathName(),
			*GetNameSafe(BehaviorTreeAsset->BlackboardAsset),
			*GetNameSafe(InPawn));
		return;
	}

	InitializeBlackboardValues(InPawn);
	RefreshTargetFromPlayers();

	if (!RunBehaviorTree(BehaviorTreeAsset))
	{
		UE_LOG(
			LogTrainingBotAIController,
			Error,
			TEXT("%s failed to start Behavior Tree %s for training bot %s."),
			*GetPathName(),
			*GetNameSafe(BehaviorTreeAsset),
			*GetNameSafe(InPawn));
		return;
	}

	StartTargetRefreshTimer();
}

void ATrainingBotAIController::OnUnPossess()
{
	StopTargetRefreshTimer();
	ClearBlackboardTarget();

	Super::OnUnPossess();
}

void ATrainingBotAIController::SetBlackboardTarget(AActor* InTarget)
{
	AEnemyBase* Enemy = Cast<AEnemyBase>(GetPawn());
	if (Enemy && !Enemy->IsActorValidAttackTarget(InTarget))
	{
		InTarget = nullptr;
	}

	UBlackboardComponent* LocalBlackboard = GetBlackboardComponent();
	if (!LocalBlackboard)
	{
		return;
	}

	if (InTarget)
	{
		const float DistanceToTarget = Enemy
			? Enemy->GetAttackDistanceToActor(InTarget)
			: GetPawn() ? FVector::Dist2D(GetPawn()->GetActorLocation(), InTarget->GetActorLocation()) : 0.0f;
		const float AttackRange = Enemy ? Enemy->GetAttackStartDistance() : 0.0f;
		const bool bHasRangedWeapon = Enemy && Enemy->IsUsingRangedWeapon();

		LocalBlackboard->SetValueAsObject(TargetActorKeyName, InTarget);
		LocalBlackboard->SetValueAsVector(LastKnownTargetLocationKeyName, InTarget->GetActorLocation());
		LocalBlackboard->SetValueAsFloat(DistanceToTargetKeyName, DistanceToTarget);
		LocalBlackboard->SetValueAsFloat(AttackRangeKeyName, AttackRange);
		LocalBlackboard->SetValueAsBool(HasRangedWeaponKeyName, bHasRangedWeapon);
		LocalBlackboard->SetValueAsBool(HasLineOfSightKeyName, true);

		if (Enemy)
		{
			Enemy->SetAttackTarget(InTarget);
		}
	}
	else
	{
		ClearBlackboardTarget();
	}
}

void ATrainingBotAIController::ClearBlackboardTarget()
{
	UBlackboardComponent* LocalBlackboard = GetBlackboardComponent();
	if (LocalBlackboard)
	{
		LocalBlackboard->ClearValue(TargetActorKeyName);
		LocalBlackboard->SetValueAsFloat(DistanceToTargetKeyName, 0.0f);
		LocalBlackboard->SetValueAsFloat(AttackRangeKeyName, 0.0f);
		LocalBlackboard->SetValueAsBool(HasRangedWeaponKeyName, false);
		LocalBlackboard->SetValueAsBool(HasLineOfSightKeyName, false);
	}

	if (AEnemyBase* Enemy = Cast<AEnemyBase>(GetPawn()))
	{
		Enemy->SetAttackTarget(nullptr);
	}
}

void ATrainingBotAIController::RefreshTargetFromPlayers()
{
	AActor* BestPlayerTarget = FindBestPlayerTarget();
	if (BestPlayerTarget)
	{
		SetBlackboardTarget(BestPlayerTarget);
		return;
	}

	ClearBlackboardTarget();
}

void ATrainingBotAIController::InitializeBlackboardValues(APawn* InPawn)
{
	UBlackboardComponent* LocalBlackboard = GetBlackboardComponent();
	if (!LocalBlackboard || !InPawn)
	{
		return;
	}

	LocalBlackboard->SetValueAsVector(SpawnLocationKeyName, InPawn->GetActorLocation());
	LocalBlackboard->SetValueAsFloat(DistanceToTargetKeyName, 0.0f);
	LocalBlackboard->SetValueAsFloat(AttackRangeKeyName, 0.0f);
	LocalBlackboard->SetValueAsBool(HasRangedWeaponKeyName, false);
	LocalBlackboard->SetValueAsBool(HasLineOfSightKeyName, false);
}

AActor* ATrainingBotAIController::FindBestPlayerTarget() const
{
	const AEnemyBase* Enemy = Cast<AEnemyBase>(GetPawn());
	const UWorld* World = GetWorld();
	if (!Enemy || !World)
	{
		return nullptr;
	}

	AActor* BestTarget = nullptr;
	float BestDistance = TNumericLimits<float>::Max();
	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		const APlayerController* PlayerController = Iterator->Get();
		APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
		if (!Enemy->IsActorValidAttackTarget(PlayerPawn))
		{
			continue;
		}

		const float Distance = Enemy->GetAttackDistanceToActor(PlayerPawn);
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			BestTarget = PlayerPawn;
		}
	}

	return BestTarget;
}

void ATrainingBotAIController::StartTargetRefreshTimer()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().ClearTimer(TargetRefreshTimerHandle);
	World->GetTimerManager().SetTimer(
		TargetRefreshTimerHandle,
		this,
		&ThisClass::RefreshTargetFromPlayers,
		FMath::Max(TargetRefreshInterval, 0.05f),
		true);
}

void ATrainingBotAIController::StopTargetRefreshTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TargetRefreshTimerHandle);
	}
}
