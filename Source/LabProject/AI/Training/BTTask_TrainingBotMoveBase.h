#pragma once

#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_TrainingBotMoveBase.generated.h"

class AAIController;

/**
 * 대상 둘레를 옆걸음으로 도는 훈련 봇 태스크의 공통 부모.
 * 거리·옆걸음·도착 판정·바라보기·내비게이션 설정과 다음 목적지 계산을 함께 쓴다.
 */
UCLASS(Abstract)
class LABPROJECT_API UBTTask_TrainingBotMoveBase : public UBTTask_BlackboardBase
{
	GENERATED_BODY()

protected:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	bool BuildMoveDestination(APawn* Pawn, AActor* TargetActor, FVector& OutDestination) const;
	void UpdateFacing(AAIController* AIController, APawn* Pawn, AActor* TargetActor, float DeltaSeconds) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float MinDistanceFromTarget = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float MaxDistanceFromTarget = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float SideStepDistance = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move")
	int32 MinSideStepMultiplier = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move")
	int32 MaxSideStepMultiplier = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float AcceptanceRadius = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float FinishDistanceTolerance = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float MaxMoveTime = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move")
	bool bStopOnOverlap = false;

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
};
