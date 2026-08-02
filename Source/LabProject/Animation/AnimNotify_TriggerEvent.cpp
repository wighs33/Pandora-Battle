#include "AnimNotify_TriggerEvent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Animation/AnimSequenceBase.h"
#include "Character/CharacterBase.h"
#include "Components/SkeletalMeshComponent.h"

void UAnimNotify_TriggerEvent::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	static_cast<void>(Animation);
	static_cast<void>(EventReference);

	// =================================================================================================================
	if (!IsValid(MeshComp))
	{

		return;
	}

	AActor* OwnerActor = MeshComp->GetOwner();
	if (!IsValid(OwnerActor) || !EventTag.IsValid())
	{

		return;
	}

	// =================================================================================================================

	UAbilitySystemComponent* AbilitySystemComponent =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OwnerActor);
	if (!AbilitySystemComponent)
	{

		return;
	}

	// =================================================================================================================

	ACharacterBase* Character = Cast<ACharacterBase>(OwnerActor);
	const bool bIsAuthoritativeNotify = OwnerActor->HasAuthority();
	const bool bIsOwningClientPresentation =
		Character && Character->IsLocallyControlled() && !bIsAuthoritativeNotify;
	if (!bIsAuthoritativeNotify && !bIsOwningClientPresentation)
	{
		return;
	}

	FGameplayEventData Payload;
	Payload.EventTag = EventTag;
	Payload.Instigator = OwnerActor;
	Payload.Target = OwnerActor;

	if (bIsAuthoritativeNotify)
	{
		// Only the server is allowed to execute the complete GameplayEvent path,
		// including abilities configured to activate from this event tag.
		AbilitySystemComponent->HandleGameplayEvent(EventTag, &Payload);
		return;
	}

	// On the owning client, notify only exact-tag listeners that are already active
	// (for predicted presentation/input preparation). Do not call HandleGameplayEvent:
	// it can activate new abilities and would turn client montage timing into gameplay.
	if (FGameplayEventMulticastDelegate* EventDelegate =
		AbilitySystemComponent->GenericGameplayEventCallbacks.Find(EventTag))
	{
		FGameplayEventMulticastDelegate DelegateCopy = *EventDelegate;
		DelegateCopy.Broadcast(&Payload);
	}
}

FString UAnimNotify_TriggerEvent::GetNotifyName_Implementation() const
{
	// =================================================================================================================

	return EventTag.IsValid()
		? FString::Printf(TEXT("TriggerEvent: %s"), *EventTag.ToString())
		: Super::GetNotifyName_Implementation();
}
