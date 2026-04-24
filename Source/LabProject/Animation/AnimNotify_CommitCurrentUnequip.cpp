#include "Animation/AnimNotify_CommitCurrentUnequip.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Equipment/EquipmentComponent.h"

void UAnimNotify_CommitCurrentUnequip::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	if (!IsValid(MeshComp))
	{
		return;
	}

	AActor* OwnerActor = MeshComp->GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		return;
	}

	if (UEquipmentComponent* EquipmentComponent = OwnerActor->FindComponentByClass<UEquipmentComponent>())
	{
		if (EquipmentComponent->UnequipCurrentItem() && EventTag.IsValid())
		{
			FGameplayEventData Payload;
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor, EventTag, Payload);
		}
	}
}

FString UAnimNotify_CommitCurrentUnequip::GetNotifyName_Implementation() const
{
	return EventTag.IsValid() ? FString::Printf(TEXT("CommitCurrentUnequip: %s"), *EventTag.ToString()) : TEXT("CommitCurrentUnequip");
}
