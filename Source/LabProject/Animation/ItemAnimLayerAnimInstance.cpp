#include "Animation/ItemAnimLayerAnimInstance.h"

#include "Character/PdPlayer.h"
#include "UObject/UnrealType.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ItemAnimLayerAnimInstance)

namespace
{
	void SetBoolPropertyIfPresent(UObject* Object, const FName PropertyName, const bool bValue)
	{
		if (!Object)
		{
			return;
		}

		FBoolProperty* BoolProperty = FindFProperty<FBoolProperty>(Object->GetClass(), PropertyName);
		if (BoolProperty)
		{
			BoolProperty->SetPropertyValue_InContainer(Object, bValue);
		}
	}

	void SetObjectPropertyIfPresent(UObject* Object, const FName PropertyName, UObject* Value)
	{
		if (!Object)
		{
			return;
		}

		FObjectPropertyBase* ObjectProperty = FindFProperty<FObjectPropertyBase>(Object->GetClass(), PropertyName);
		if (ObjectProperty)
		{
			ObjectProperty->SetObjectPropertyValue_InContainer(Object, Value);
		}
	}
}

void UItemAnimLayerAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	RefreshCachedPlayer();
	bIsAiming = IsValid(CachedPlayer.Get()) && CachedPlayer->IsWeaponAimActive();
	PushValuesToLegacyBlueprintVariables();
}

void UItemAnimLayerAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!IsValid(CachedPlayer.Get()))
	{
		RefreshCachedPlayer();
	}

	bIsAiming = IsValid(CachedPlayer.Get()) && CachedPlayer->IsWeaponAimActive();
	PushValuesToLegacyBlueprintVariables();
}

void UItemAnimLayerAnimInstance::RefreshCachedPlayer()
{
	CachedPlayer = Cast<APdPlayer>(TryGetPawnOwner());
	if (!IsValid(CachedPlayer.Get()))
	{
		CachedPlayer = Cast<APdPlayer>(GetOwningActor());
	}
}

void UItemAnimLayerAnimInstance::PushValuesToLegacyBlueprintVariables()
{
	SetObjectPropertyIfPresent(this, TEXT("Cached Player"), CachedPlayer.Get());
	SetObjectPropertyIfPresent(this, TEXT("CachedPlayer"), CachedPlayer.Get());

	SetBoolPropertyIfPresent(this, TEXT("IsAiming?"), bIsAiming);
	SetBoolPropertyIfPresent(this, TEXT("IsAiming"), bIsAiming);
}
