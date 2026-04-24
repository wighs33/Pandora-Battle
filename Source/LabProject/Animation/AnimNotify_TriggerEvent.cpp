#include "AnimNotify_TriggerEvent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"

void UAnimNotify_TriggerEvent::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	if (!IsValid(MeshComp))
	{
		return;
	}

	AActor* OwnerActor = MeshComp->GetOwner();
	if (!IsValid(OwnerActor) || !EventTag.IsValid())
	{
		return;
	}

	if (!UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OwnerActor))
	{
		return;
	}

	FGameplayEventData Payload;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor, EventTag, Payload);
}

FString UAnimNotify_TriggerEvent::GetNotifyName_Implementation() const
{
	return EventTag.IsValid() ? FString::Printf(TEXT("TriggerEvent: %s"), *EventTag.ToString()) : Super::GetNotifyName_Implementation();
}
