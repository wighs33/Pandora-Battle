#include "Animation/AnimNotify_WeaponEvent.h"

#include "Animation/AnimSequenceBase.h"
#include "Character/PdPlayer.h"
#include "Components/SkeletalMeshComponent.h"
#include "Component/Player/EquipmentComponent.h"
#include "Weapon/WeaponBase.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNotify_WeaponEvent)

UAnimNotify_WeaponEvent::UAnimNotify_WeaponEvent()
{
	WeaponEventName = NAME_None;
}

void UAnimNotify_WeaponEvent::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	static_cast<void>(Animation);
	static_cast<void>(EventReference);

	DispatchWeaponEvent(MeshComp, WeaponEventName);
}

FString UAnimNotify_WeaponEvent::GetNotifyName_Implementation() const
{
	return WeaponEventName.IsNone() ? TEXT("WeaponEvent") : WeaponEventName.ToString();
}

bool UAnimNotify_WeaponEvent::DispatchWeaponEvent(USkeletalMeshComponent* MeshComp, FName EventName)
{
	if (!IsValid(MeshComp) || EventName.IsNone())
	{
		return false;
	}

	APdPlayer* PlayerCharacter = Cast<APdPlayer>(MeshComp->GetOwner());
	if (!PlayerCharacter)
	{
		return false;
	}

	UEquipmentComponent* EquipmentComponent = PlayerCharacter->GetEquipmentComponent();
	AWeaponBase* WeaponActor = EquipmentComponent ? EquipmentComponent->GetCurrentWeaponActor() : nullptr;
	const bool bHandled = WeaponActor ? WeaponActor->OnWeaponAnimNotifyTiming(EventName, PlayerCharacter) : false;
	return bHandled;
}
