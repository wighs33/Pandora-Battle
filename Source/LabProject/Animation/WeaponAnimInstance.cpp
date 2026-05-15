#include "Animation/WeaponAnimInstance.h"

#include "Animation/AnimMontage.h"
#include "Character/PdPlayer.h"
#include "Common/WeaponAnimNotifyNames.h"
#include "Weapon/WeaponBase.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(WeaponAnimInstance)

void UWeaponAnimInstance::AnimNotify_HoldBow()
{
	AWeaponBase* WeaponActor = Cast<AWeaponBase>(GetOwningActor());
	APdPlayer* PlayerCharacter = WeaponActor ? Cast<APdPlayer>(WeaponActor->GetOwner()) : nullptr;
	if (WeaponActor && PlayerCharacter)
	{
		WeaponActor->OnWeaponAnimNotifyTiming(WeaponAnimNotifyNames::HoldBow(), PlayerCharacter);
	}

	UAnimMontage* MontageToPause = HoldBowMontage ? HoldBowMontage.Get() : GetCurrentActiveMontage();
	if (MontageToPause)
	{
		Montage_Pause(MontageToPause);
		return;
	}

	Montage_Pause(nullptr);
}
