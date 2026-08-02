#pragma once

#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_MoveAroundTarget.generated.h"

UCLASS()
class LABPROJECT_API UBTTask_MoveAroundTarget : public UBTTask_BlackboardBase
{
	GENERATED_BODY()

public:
	UBTTask_MoveAroundTarget();

	virtual uint16 GetInstanceMemorySize() const override;
	virtual FString GetStaticDescription() const override;

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move Around Target", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float MinDistanceFromTarget = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move Around Target", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float MaxDistanceFromTarget = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move Around Target", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float SideStepDistance = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move Around Target")
	int32 MinSideStepMultiplier = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move Around Target")
	int32 MaxSideStepMultiplier = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move Around Target", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float AcceptanceRadius = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move Around Target", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float FinishDistanceTolerance = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move Around Target", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float MaxMoveTime = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move Around Target")
	bool bTreatTimeoutAsSuccess = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move Around Target")
	bool bStopOnOverlap = false;

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
	bool bFaceTargetWhileMoving = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facing")
	bool bKeepFacingTargetAfterMove = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facing", meta = (ClampMin = "0.0", ForceUnits = "deg/s"))
	float FaceTargetRotationInterpSpeed = 720.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation")
	bool bProjectDestinationToNavigation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float NavigationProjectionExtent = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDrawDebug = false;

private:
	EBTNodeResult::Type RequestNextMove(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, APawn* Pawn, AActor* TargetActor);
	EBTNodeResult::Type CompleteOneMoveAndMaybeContinue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, APawn* Pawn, AActor* TargetActor);
	EBTNodeResult::Type RequestAttackApproach(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, APawn* Pawn, AActor* TargetActor);
	EBTNodeResult::Type TryFinishAttackApproach(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, APawn* Pawn, AActor* TargetActor);
	EBTNodeResult::Type StartAttackWindow(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, APawn* Pawn, AActor* TargetActor);
	EBTNodeResult::Type TickAttackWindow(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, APawn* Pawn, AActor* TargetActor);
	bool TryAttackAfterMoves(AAIController* AIController, APawn* Pawn, AActor* TargetActor) const;
	bool BuildMoveDestination(APawn* Pawn, AActor* TargetActor, FVector& OutDestination) const;
	void ApplyFacingMode(APawn* Pawn, uint8* NodeMemory) const;
	void UpdateFacing(AAIController* AIController, APawn* Pawn, AActor* TargetActor, float DeltaSeconds) const;
	void RestoreMovementSettings(APawn* Pawn, uint8* NodeMemory, bool bRestoreFacing) const;
};
