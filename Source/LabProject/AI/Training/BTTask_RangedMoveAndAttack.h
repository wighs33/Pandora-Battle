#pragma once

#include "AI/Training/BTTask_TrainingBotMoveBase.h"
#include "BTTask_RangedMoveAndAttack.generated.h"

UCLASS()
class LABPROJECT_API UBTTask_RangedMoveAndAttack : public UBTTask_TrainingBotMoveBase
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual uint16 GetInstanceMemorySize() const override;
	virtual FString GetStaticDescription() const override;

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;

public:
	// Public API ------------------------------------------------------------------------------------------------------
	UBTTask_RangedMoveAndAttack();

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	EBTNodeResult::Type RequestMove(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, APawn* Pawn, AActor* TargetActor);
	EBTNodeResult::Type TickMove(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, APawn* Pawn, AActor* TargetActor);
	bool RequestRetreatMove(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, APawn* Pawn, AActor* TargetActor);
	bool TryAttack(AAIController* AIController, APawn* Pawn, AActor* TargetActor) const;
	bool BuildRetreatDestination(APawn* Pawn, AActor* TargetActor, FVector& OutDestination) const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float RetreatDistance = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float RetreatMoveDistance = 700.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move", meta = (ClampMin = "0.05", ForceUnits = "s"))
	float RetreatRepathInterval = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	bool bAttackWhileMoving = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack", meta = (ClampMin = "0.05", ForceUnits = "s"))
	float AttackRequestInterval = 0.75f;
};
