#pragma once

#include "AI/Training/BTTask_TrainingBotMoveBase.h"
#include "BTTask_MoveAroundTarget.generated.h"

UCLASS()
class LABPROJECT_API UBTTask_MoveAroundTarget : public UBTTask_TrainingBotMoveBase
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
	UBTTask_MoveAroundTarget();

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	EBTNodeResult::Type RequestNextMove(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, APawn* Pawn, AActor* TargetActor);
	EBTNodeResult::Type CompleteOneMoveAndMaybeContinue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, APawn* Pawn, AActor* TargetActor);
	EBTNodeResult::Type RequestAttackApproach(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, APawn* Pawn, AActor* TargetActor);
	EBTNodeResult::Type TryFinishAttackApproach(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, APawn* Pawn, AActor* TargetActor);
	EBTNodeResult::Type StartAttackWindow(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, APawn* Pawn, AActor* TargetActor);
	EBTNodeResult::Type TickAttackWindow(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, APawn* Pawn, AActor* TargetActor);
	bool TryAttackAfterMoves(AAIController* AIController, APawn* Pawn, AActor* TargetActor) const;
	void ApplyFacingMode(APawn* Pawn, uint8* NodeMemory) const;
	void RestoreMovementSettings(APawn* Pawn, uint8* NodeMemory, bool bRestoreFacing) const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move Around Target")
	bool bTreatTimeoutAsSuccess = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move Around Target", meta = (ClampMin = "1"))
	int32 MinMovesBeforeAttack = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move Around Target", meta = (ClampMin = "1"))
	int32 MaxMovesBeforeAttack = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	bool bAttackAfterMoves = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	bool bApproachTargetBeforeAttack = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	bool bStopMovementBeforeAttack = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float MaxAttackApproachTime = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	bool bTreatAttackApproachTimeoutAsSuccess = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float MaxComboAttackWaitTime = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack", meta = (ClampMin = "0.05", ForceUnits = "s"))
	float ComboAttackRequestInterval = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facing")
	bool bKeepFacingTargetAfterMove = true;
};
