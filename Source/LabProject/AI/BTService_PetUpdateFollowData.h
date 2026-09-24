#pragma once

#include "BehaviorTree/BTService.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Float.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BTService_PetUpdateFollowData.generated.h"

UCLASS()
class LABPROJECT_API UBTService_PetUpdateFollowData : public UBTService
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;

protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

public:
	// Public API ------------------------------------------------------------------------------------------------------
	UBTService_PetUpdateFollowData();

protected:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	AActor* ResolveFollowTarget(const APawn* Pawn, UBlackboardComponent* BlackboardComponent) const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Pet|Blackboard")
	FBlackboardKeySelector FollowTargetActorKey;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Pet|Blackboard")
	FBlackboardKeySelector DistanceToOwnerKey;
};
