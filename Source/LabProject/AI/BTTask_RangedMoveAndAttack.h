#pragma once

#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_RangedMoveAndAttack.generated.h"

UCLASS()
class LABPROJECT_API UBTTask_RangedMoveAndAttack : public UBTTask_BlackboardBase
{
	GENERATED_BODY()

public:
	UBTTask_RangedMoveAndAttack();

	virtual uint16 GetInstanceMemorySize() const override;
	virtual FString GetStaticDescription() const override;

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float MinDistanceFromTarget = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float MaxDistanceFromTarget = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float SideStepDistance = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move")
	int32 MinSideStepMultiplier = -2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move")
	int32 MaxSideStepMultiplier = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float AcceptanceRadius = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float FinishDistanceTolerance = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float MaxMoveTime = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float RetreatDistance = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float RetreatMoveDistance = 700.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move", meta = (ClampMin = "0.05", ForceUnits = "s"))
	float RetreatRepathInterval = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move")
	bool bStopOnOverlap = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	bool bAttackWhileMoving = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack", meta = (ClampMin = "0.05", ForceUnits = "s"))
	float AttackRequestInterval = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facing")
	bool bFaceTargetWhileMoving = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facing", meta = (ClampMin = "0.0", ForceUnits = "deg/s"))
	float FaceTargetRotationInterpSpeed = 720.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation")
	bool bProjectDestinationToNavigation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float NavigationProjectionExtent = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDrawDebug = false;

private:
	EBTNodeResult::Type RequestMove(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, APawn* Pawn, AActor* TargetActor);
	EBTNodeResult::Type TickMove(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, APawn* Pawn, AActor* TargetActor);
	bool RequestRetreatMove(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, APawn* Pawn, AActor* TargetActor);
	bool TryAttack(AAIController* AIController, APawn* Pawn, AActor* TargetActor) const;
	bool BuildMoveDestination(APawn* Pawn, AActor* TargetActor, FVector& OutDestination) const;
	bool BuildRetreatDestination(APawn* Pawn, AActor* TargetActor, FVector& OutDestination) const;
	void UpdateFacing(AAIController* AIController, APawn* Pawn, AActor* TargetActor, float DeltaSeconds) const;
};
