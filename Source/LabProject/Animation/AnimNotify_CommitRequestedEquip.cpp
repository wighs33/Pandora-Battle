#include "Animation/AnimNotify_CommitRequestedEquip.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Equipment/EquipmentComponent.h"

void UAnimNotify_CommitRequestedEquip::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	if (!IsValid(MeshComp))
	{
		return;
	}

	AActor* OwnerActor = MeshComp->GetOwner();
	if (!IsValid(OwnerActor))
	{
		return;
	}

	if (UEquipmentComponent* EquipmentComponent = OwnerActor->FindComponentByClass<UEquipmentComponent>())
	{
		if (EquipmentComponent->EquipRequestedItem() && EventTag.IsValid())
		{
			FGameplayEventData Payload;
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor, EventTag, Payload);
		}
	}
}

FString UAnimNotify_CommitRequestedEquip::GetNotifyName_Implementation() const
{
	return EventTag.IsValid() ? FString::Printf(TEXT("CommitRequestedEquip: %s"), *EventTag.ToString()) : TEXT("CommitRequestedEquip");
}
