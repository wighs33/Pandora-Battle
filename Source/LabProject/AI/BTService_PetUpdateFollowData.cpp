#include "AI/BTService_PetUpdateFollowData.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTree.h"
#include "Pet/PetCharacter.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BTService_PetUpdateFollowData)

UBTService_PetUpdateFollowData::UBTService_PetUpdateFollowData()
{
	NodeName = TEXT("Pet Update Follow Data");
	Interval = 0.15f;
	RandomDeviation = 0.0f;

	FollowTargetActorKey.SelectedKeyName = TEXT("FollowTargetActor");
	FollowTargetActorKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_PetUpdateFollowData, FollowTargetActorKey), AActor::StaticClass());

	DistanceToOwnerKey.SelectedKeyName = TEXT("DistanceToOwner");
	DistanceToOwnerKey.AddFloatFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_PetUpdateFollowData, DistanceToOwnerKey));
}

void UBTService_PetUpdateFollowData::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	if (UBlackboardData* BlackboardAsset = GetBlackboardAsset())
	{
		FollowTargetActorKey.ResolveSelectedKey(*BlackboardAsset);
		DistanceToOwnerKey.ResolveSelectedKey(*BlackboardAsset);
	}
}

void UBTService_PetUpdateFollowData::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, const float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	AAIController* AIController = OwnerComp.GetAIOwner();
	APawn* Pawn = AIController ? AIController->GetPawn() : nullptr;
	UBlackboardComponent* BlackboardComponent = OwnerComp.GetBlackboardComponent();
	if (!Pawn || !BlackboardComponent)
	{
		return;
	}

	AActor* FollowTarget = ResolveFollowTarget(Pawn, BlackboardComponent);
	if (FollowTarget)
	{
		BlackboardComponent->SetValueAsObject(FollowTargetActorKey.SelectedKeyName, FollowTarget);
		BlackboardComponent->SetValueAsFloat(
			DistanceToOwnerKey.SelectedKeyName,
			FVector::Dist2D(Pawn->GetActorLocation(), FollowTarget->GetActorLocation()));
	}
	else
	{
		BlackboardComponent->ClearValue(FollowTargetActorKey.SelectedKeyName);
		BlackboardComponent->SetValueAsFloat(DistanceToOwnerKey.SelectedKeyName, 0.0f);
	}
}

AActor* UBTService_PetUpdateFollowData::ResolveFollowTarget(const APawn* Pawn, UBlackboardComponent* BlackboardComponent) const
{
	if (!Pawn)
	{
		return nullptr;
	}

	if (BlackboardComponent)
	{
		if (AActor* BlackboardTarget = Cast<AActor>(BlackboardComponent->GetValueAsObject(FollowTargetActorKey.SelectedKeyName)))
		{
			return BlackboardTarget;
		}
	}

	if (const APetCharacter* PetCharacter = Cast<APetCharacter>(Pawn))
	{
		if (AActor* FollowTarget = PetCharacter->GetFollowTargetActor())
		{
			return FollowTarget;
		}

		return PetCharacter->GetOwner();
	}

	return Pawn->GetOwner();
}
