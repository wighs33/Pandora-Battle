#include "Animation/ItemAnimLayerAnimInstance.h"

#include "Animation/AnimNode_AssetPlayerBase.h"
#include "Animation/BlendSpace.h"
#include "Character/PdPlayer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ItemAnimLayerAnimInstance)

void UItemAnimLayerAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	RefreshCachedPlayer();
	bIsAiming = IsValid(CachedPlayer.Get()) && CachedPlayer->IsWeaponAimActive();
}

void UItemAnimLayerAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!IsValid(CachedPlayer.Get()))
	{
		RefreshCachedPlayer();
	}

	bIsAiming = IsValid(CachedPlayer.Get()) && CachedPlayer->IsWeaponAimActive();
	UpdateLocomotionPlayRate();
}

void UItemAnimLayerAnimInstance::RefreshCachedPlayer()
{
	CachedPlayer = Cast<APdPlayer>(TryGetPawnOwner());
	if (!IsValid(CachedPlayer.Get()))
	{
		CachedPlayer = Cast<APdPlayer>(GetOwningActor());
	}
}

// 블렌드스페이스는 축 끝(최고 속도)에서 멈추고 그보다 빨라져도 애니를 빨리 돌리지 않는다. 그래서 무기의 달리기 애니가 만들어진
// 속도보다 빨리 움직이면 발이 미끄러졌다(도끼 달리기 265 cm/s로 460 cm/s 이동). 넘는 비율만큼 재생 속도를 올린다.
void UItemAnimLayerAnimInstance::UpdateLocomotionPlayRate()
{
	const float GroundSpeed = IsValid(CachedPlayer.Get()) ? CachedPlayer->GetVelocity().Size2D() : 0.0f;
	const float TopSpeed = FindLocomotionTopSpeed();
	LocomotionPlayRate = TopSpeed > 0.0f && GroundSpeed > TopSpeed ? FMath::Min(GroundSpeed / TopSpeed, MaxLocomotionPlayRate) : 1.0f;
}

// 무기 레이어가 바꿔 끼우는 이동 블렌드스페이스는 블루프린트 변수라, 지금 재생 중인 노드에서 속도 축의 최댓값을 읽는다.
float UItemAnimLayerAnimInstance::FindLocomotionTopSpeed() const
{
	for (const FAnimNode_AssetPlayerBase* Player : GetInstanceAssetPlayers(LocomotionGraphName))
	{
		const UBlendSpace* Space = Player ? Cast<UBlendSpace>(Player->GetAnimAsset()) : nullptr;
		for (int32 Axis = 0; Space && Axis < 3; ++Axis)
		{
			const FBlendParameter& Parameter = Space->GetBlendParameter(Axis);
			if (Parameter.DisplayName == SpeedAxisName)
			{
				return Parameter.Max;
			}
		}
	}
	return 0.0f;
}
