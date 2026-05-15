#include "Animation/AnimNotify_RedrawBow.h"

#include "Common/WeaponAnimNotifyNames.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNotify_RedrawBow)

UAnimNotify_RedrawBow::UAnimNotify_RedrawBow()
{
	WeaponEventName = WeaponAnimNotifyNames::RedrawBow();
}

FString UAnimNotify_RedrawBow::GetNotifyName_Implementation() const
{
	return WeaponAnimNotifyNames::RedrawBow().ToString();
}
