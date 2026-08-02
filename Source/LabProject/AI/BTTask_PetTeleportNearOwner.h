#pragma once

#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_PetTeleportNearOwner.generated.h"

UCLASS()
class LABPROJECT_API UBTTask_PetTeleportNearOwner : public UBTTask_BlackboardBase
{
	GENERATED_BODY()

public:
	UBTTask_PetTeleportNearOwner();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

protected:
	FVector ResolveTeleportLocation(const APawn* Pawn, const AActor* FollowTarget) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Pet|Teleport")
	FVector TeleportOffset = FVector(-180.0f, 100.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Pet|Teleport")
	bool bUseTargetRotationForOffset = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Pet|Teleport")
	bool bProjectToNavigation = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Pet|Teleport", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float NavigationProjectionExtent = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Pet|Teleport")
	bool bMatchTargetYaw = true;
};
