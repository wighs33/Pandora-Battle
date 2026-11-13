#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "ItemAnimLayerAnimInstance.generated.h"

class APdPlayer;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UItemAnimLayerAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	void RefreshCachedPlayer();
	void UpdateLocomotionPlayRate();
	float FindLocomotionTopSpeed() const;

protected:
	UPROPERTY(Transient, BlueprintReadOnly, Category = "!Animation|References")
	TObjectPtr<APdPlayer> CachedPlayer = nullptr;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "!Animation|State")
	bool bIsAiming = false;

	/**
	 * 이동 블렌드스페이스 재생 배율. 블렌드스페이스의 최고 속도보다 빨리 달리면(이동 속도 증가, 느린 달리기 애니를 쓰는 무기)
	 * 그 비율만큼 빨리 재생해 발이 땅에서 미끄러지지 않게 한다. 최고 속도 이하에서는 1이다.
	 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "!Animation|Locomotion")
	float LocomotionPlayRate = 1.0f;

	/** 이동 블렌드스페이스를 재생하는 레이어 그래프 이름. 이 그래프의 블렌드스페이스에서 최고 속도를 읽는다. */
	UPROPERTY(EditDefaultsOnly, Category = "!Animation|Locomotion")
	FName LocomotionGraphName = TEXT("Loco");

	/** 블렌드스페이스에서 이동 속도를 나타내는 축의 이름. */
	UPROPERTY(EditDefaultsOnly, Category = "!Animation|Locomotion")
	FString SpeedAxisName = TEXT("Speed");

	/** 재생 배율의 상한. 순간적으로 튀는 속도(넉백, 돌진)에 다리가 지나치게 빨리 돌지 않게 한다. */
	UPROPERTY(EditDefaultsOnly, Category = "!Animation|Locomotion", meta = (ClampMin = "1.0"))
	float MaxLocomotionPlayRate = 2.5f;
};
