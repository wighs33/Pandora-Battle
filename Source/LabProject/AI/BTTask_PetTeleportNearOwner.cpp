#include "AI/BTTask_PetTeleportNearOwner.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Pawn.h"
#include "NavigationSystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BTTask_PetTeleportNearOwner)

UBTTask_PetTeleportNearOwner::UBTTask_PetTeleportNearOwner()
{
	NodeName = TEXT("Pet Teleport Near Owner");

	BlackboardKey.SelectedKeyName = TEXT("FollowTargetActor");
	BlackboardKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_PetTeleportNearOwner, BlackboardKey), AActor::StaticClass());
}

EBTNodeResult::Type UBTTask_PetTeleportNearOwner::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	static_cast<void>(NodeMemory);

	AAIController* AIController = OwnerComp.GetAIOwner();
	APawn* Pawn = AIController ? AIController->GetPawn() : nullptr;
	UBlackboardComponent* BlackboardComponent = OwnerComp.GetBlackboardComponent();
	AActor* FollowTarget = BlackboardComponent
		? Cast<AActor>(BlackboardComponent->GetValueAsObject(BlackboardKey.SelectedKeyName))
		: nullptr;
	if (!AIController || !Pawn || !FollowTarget)
	{
		return EBTNodeResult::Failed;
	}

	AIController->StopMovement();

	const FVector TeleportLocation = ResolveTeleportLocation(Pawn, FollowTarget);
	Pawn->SetActorLocation(TeleportLocation, false, nullptr, ETeleportType::TeleportPhysics);
	if (bMatchTargetYaw)
	{
		const FRotator TargetRotation(0.0f, FollowTarget->GetActorRotation().Yaw, 0.0f);
		Pawn->SetActorRotation(TargetRotation);
	}

	return EBTNodeResult::Succeeded;
}

FVector UBTTask_PetTeleportNearOwner::ResolveTeleportLocation(const APawn* Pawn, const AActor* FollowTarget) const
{
	if (!Pawn || !FollowTarget)
	{
		return FVector::ZeroVector;
	}

	const FVector ResolvedOffset = bUseTargetRotationForOffset
		? FollowTarget->GetActorRotation().RotateVector(TeleportOffset)
		: TeleportOffset;
	FVector DesiredLocation = FollowTarget->GetActorLocation() + ResolvedOffset;

	if (bProjectToNavigation)
	{
		if (const UWorld* World = Pawn->GetWorld())
		{
			if (const UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
			{
				FNavLocation ProjectedLocation;
				const FVector ProjectionExtent(NavigationProjectionExtent, NavigationProjectionExtent, NavigationProjectionExtent);
				if (NavigationSystem->ProjectPointToNavigation(DesiredLocation, ProjectedLocation, ProjectionExtent))
				{
					DesiredLocation = ProjectedLocation.Location;
				}
			}
		}
	}

	return DesiredLocation;
}
