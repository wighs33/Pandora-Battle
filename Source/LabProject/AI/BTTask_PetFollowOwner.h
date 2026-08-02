#pragma once

#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_PetFollowOwner.generated.h"

USTRUCT()
struct FPdPetFollowOwnerTaskMemory
{
	GENERATED_BODY()

	float TimeSinceLastMoveRequest = 0.0f;
};

UCLASS()
class LABPROJECT_API UBTTask_PetFollowOwner : public UBTTask_BlackboardBase
{
	GENERATED_BODY()

public:
	UBTTask_PetFollowOwner();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;
	virtual uint16 GetInstanceMemorySize() const override;

protected:
	bool UpdateFollowMove(UBehaviorTreeComponent& OwnerComp, FPdPetFollowOwnerTaskMemory& TaskMemory, float DeltaSeconds) const;
	FVector ResolveFollowDestination(const APawn* Pawn, const AActor* FollowTarget) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Pet|Follow", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float AcceptanceRadius = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Pet|Follow")
	FVector FollowOffset = FVector(-140.0f, 80.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Pet|Follow", meta = (ClampMin = "0.05", ForceUnits = "s"))
	float RepathInterval = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Pet|Follow", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float TeleportDistance = 2500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Pet|Follow")
	bool bUseTargetRotationForOffset = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Pet|Follow")
	bool bStopMovementInsideAcceptanceRadius = true;
};
