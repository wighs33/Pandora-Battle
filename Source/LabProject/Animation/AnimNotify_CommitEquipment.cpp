#include "Animation/AnimNotify_CommitEquipment.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Animation/AnimSequenceBase.h"
#include "Character/CharacterBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Component/Player/EquipmentComponent.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNotify_CommitEquipment)

void UAnimNotify_CommitEquipment::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	static_cast<void>(Animation);
	static_cast<void>(EventReference);

	AActor* OwnerActor = IsValid(MeshComp) ? MeshComp->GetOwner() : nullptr;
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		return;
	}

	ACharacterBase* CharacterOwner = Cast<ACharacterBase>(OwnerActor);
	UEquipmentComponent* EquipmentComponent = CharacterOwner ? CharacterOwner->GetEquipmentComponent() : nullptr;
	if (EquipmentComponent && CommitEquipment(*EquipmentComponent) && EventTag.IsValid())
	{
		FGameplayEventData Payload;
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor, EventTag, Payload);
	}
}

FString UAnimNotify_CommitEquipment::GetNotifyName_Implementation() const
{
	return EventTag.IsValid()
		? FString::Printf(TEXT("%s: %s"), *GetCommitName(), *EventTag.ToString())
		: GetCommitName();
}
