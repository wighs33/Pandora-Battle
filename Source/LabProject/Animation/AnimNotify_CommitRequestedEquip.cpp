#include "Animation/AnimNotify_CommitRequestedEquip.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Animation/AnimSequenceBase.h"
#include "Character/CharacterBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Component/Player/EquipmentComponent.h"

void UAnimNotify_CommitRequestedEquip::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	static_cast<void>(Animation);
	static_cast<void>(EventReference);

	// =================================================================================================================
	if (!IsValid(MeshComp))
	{

		return;
	}

	AActor* OwnerActor = MeshComp->GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{

		return;
	}

	// =================================================================================================================

	ACharacterBase* CharacterOwner = Cast<ACharacterBase>(OwnerActor);
	UEquipmentComponent* EquipmentComponent = CharacterOwner ? CharacterOwner->GetEquipmentComponent() : nullptr;
	if (EquipmentComponent)
	{
		const bool bEquipped = EquipmentComponent->EquipWeapon();

		if (bEquipped && EventTag.IsValid())
		{
			// =================================================================================================================

			FGameplayEventData Payload;
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor, EventTag, Payload);

		}
	}
}

FString UAnimNotify_CommitRequestedEquip::GetNotifyName_Implementation() const
{
	// =================================================================================================================

	return EventTag.IsValid()
		? FString::Printf(TEXT("CommitRequestedEquip: %s"), *EventTag.ToString())
		: TEXT("CommitRequestedEquip");
}
