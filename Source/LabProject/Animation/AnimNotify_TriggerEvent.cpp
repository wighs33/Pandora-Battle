#include "AnimNotify_TriggerEvent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Animation/AnimSequenceBase.h"
#include "Character/PdCharacterBase.h"
#include "Components/SkeletalMeshComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogAnimNotifyTriggerEvent, Log, All);

/** ? ë‹˜ ?¸í‹°?Œì´ ?œì ??ì§€?•í•œ ê²Œì„?Œë ˆ???´ë²¤?¸ë? ?„ì†¡?©ë‹ˆ?? */
void UAnimNotify_TriggerEvent::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	// ?¬ìš©?˜ì? ?ŠëŠ” ?¸ì ê²½ê³ ë¥?ë°©ì??©ë‹ˆ??
	static_cast<void>(Animation);
	static_cast<void>(EventReference);

	// =================================================================================================================
	// === ê¸°ë³¸ ? íš¨??ê²€??
	if (!IsValid(MeshComp))
	{
		UE_LOG(LogAnimNotifyTriggerEvent, Warning,
			TEXT("Notify skipped: MeshComp is invalid. notify=%s eventTag=%s"),
			*GetNameSafe(this),
			*EventTag.ToString());
		return;
	}

	AActor* OwnerActor = MeshComp->GetOwner();
	if (!IsValid(OwnerActor) || !EventTag.IsValid())
	{
		UE_LOG(LogAnimNotifyTriggerEvent, Warning,
			TEXT("Notify skipped: owner/tag invalid. notify=%s owner=%s eventTag=%s animation=%s"),
			*GetNameSafe(this),
			*GetNameSafe(OwnerActor),
			*EventTag.ToString(),
			*GetNameSafe(Animation));
		return;
	}

	// =================================================================================================================
	// === AbilitySystemComponent ì¡´ì¬ ?¬ë? ?•ì¸

	if (!UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OwnerActor))
	{
		UE_LOG(LogAnimNotifyTriggerEvent, Warning,
			TEXT("Notify skipped: owner has no ASC. owner=%s eventTag=%s animation=%s"),
			*GetNameSafe(OwnerActor),
			*EventTag.ToString(),
			*GetNameSafe(Animation));
		return;
	}

	// =================================================================================================================
	// === ê²Œì„?Œë ˆ???´ë²¤???„ì†¡

	APdCharacterBase* Character = Cast<APdCharacterBase>(OwnerActor);
	FGameplayEventData Payload;
	Payload.EventTag = EventTag;
	Payload.Instigator = OwnerActor;
	Payload.Target = OwnerActor;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor, EventTag, Payload);
	UE_LOG(LogAnimNotifyTriggerEvent, Log,
		TEXT("Notify sent gameplay event locally. owner=%s eventTag=%s animation=%s authority=%s localControlled=%s"),
		*GetNameSafe(OwnerActor),
		*EventTag.ToString(),
		*GetNameSafe(Animation),
		OwnerActor->HasAuthority() ? TEXT("true") : TEXT("false"),
		Character && Character->IsLocallyControlled() ? TEXT("true") : TEXT("false"));
	if (Character && Character->IsLocallyControlled() && !Character->HasAuthority())
	{
		Character->ServerSendGameplayEventToSelf(Payload);
		UE_LOG(LogAnimNotifyTriggerEvent, Log,
			TEXT("Notify forwarded gameplay event to server. owner=%s eventTag=%s animation=%s"),
			*GetNameSafe(OwnerActor),
			*EventTag.ToString(),
			*GetNameSafe(Animation));
	}
}

/** ?ë””?°ì? ?”ë²„ê·¸ì—???œì‹œ???¸í‹°?Œì´ ?´ë¦„??ë°˜í™˜?©ë‹ˆ?? */
FString UAnimNotify_TriggerEvent::GetNotifyName_Implementation() const
{
	// =================================================================================================================
	// === ?´ë²¤???œê·¸ ?¬í•¨ ?¸í‹°?Œì´ ?´ë¦„ êµ¬ì„±

	return EventTag.IsValid()
		? FString::Printf(TEXT("TriggerEvent: %s"), *EventTag.ToString())
		: Super::GetNotifyName_Implementation();
}