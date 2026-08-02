#include "Animation/AnimNotify_CommitCurrentUnequip.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Animation/AnimSequenceBase.h"
#include "Character/CharacterBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Component/Player/EquipmentComponent.h"

void UAnimNotify_CommitCurrentUnequip::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	// =================================================================================================================
	static_cast<void>(Animation);
	static_cast<void>(EventReference);

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
		const bool bUnequipped = EquipmentComponent->UnequipCurrentWeapon();


		if (bUnequipped && EventTag.IsValid())
		{
			// =================================================================================================================

			FGameplayEventData Payload;
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor, EventTag, Payload);

		}
	}
}

FString UAnimNotify_CommitCurrentUnequip::GetNotifyName_Implementation() const
{
	// =================================================================================================================

	return EventTag.IsValid()
		? FString::Printf(TEXT("CommitCurrentUnequip: %s"), *EventTag.ToString())
		: TEXT("CommitCurrentUnequip");
}
