#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_LeapToTarget.generated.h"

class UCurveFloat;

/**
 * 구간 동안 공격 대상 앞까지 실제로 이동한다. 서버가 루트 모션 소스로 캡슐을 옮기므로 충돌과 원격 화면 동기화는 이동 컴포넌트가 맡는다.
 * 애니메이션의 전진 이동을 뺀 제자리 애니메이션과 함께 쓰고, 뺀 전진 이동의 시간 곡선을 TravelCurve로 넣어 발 디딤과 이동 속도를 맞춘다.
 */
UCLASS(meta = (DisplayName = "Leap To Target"))
class LABPROJECT_API UAnimNotifyState_LeapToTarget : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NotifyBegin(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		float TotalDuration,
		const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

protected:
	/** 구간 진행 비율(0~1)에 따른 이동 비율(0~1). 비어 있으면 일정한 속도로 이동한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Leap")
	TObjectPtr<UCurveFloat> TravelCurve;

	/** 도착했을 때 대상과 남길 거리(두 캡슐 가장자리 사이). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Leap", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float StopDistance = 300.0f;

	/** 한 번에 이동하는 최대 거리. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Leap", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float MaxDistance = 1500.0f;
};
