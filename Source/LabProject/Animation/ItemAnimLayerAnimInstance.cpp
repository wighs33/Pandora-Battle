#include "Animation/ItemAnimLayerAnimInstance.h"

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
}

void UItemAnimLayerAnimInstance::RefreshCachedPlayer()
{
	CachedPlayer = Cast<APdPlayer>(TryGetPawnOwner());
	if (!IsValid(CachedPlayer.Get()))
	{
		CachedPlayer = Cast<APdPlayer>(GetOwningActor());
	}
}
