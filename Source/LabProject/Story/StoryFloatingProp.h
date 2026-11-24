#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMeshActor.h"
#include "StoryFloatingProp.generated.h"

/** 떠다니는 무대 소품. 놓인 자리를 중심으로 오르내리고, 돌고, 커졌다 작아진다. */
UCLASS()
class LABPROJECT_API AStoryFloatingProp : public AStaticMeshActor
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// Public API ------------------------------------------------------------------------------------------------------
	AStoryFloatingProp(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Story|Motion", meta = (Units = "cm"))
	float BobHeight = 0.f;

	/** 한 번 오르내리거나 커졌다 작아지는 시간 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Story|Motion", meta = (ClampMin = "0.1", Units = "s"))
	float PeriodSeconds = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Story|Motion")
	float SpinDegreesPerSecond = 0.f;

	/** 크기가 바뀌는 비율(0.4면 0.6~1.4배) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Story|Motion", meta = (ClampMin = "0.0", ClampMax = "0.95"))
	float PulseScale = 0.f;

private:
	FTransform PlacedTransform;
	float Elapsed = 0.f;
};
