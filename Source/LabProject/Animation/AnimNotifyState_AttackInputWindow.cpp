#include "Animation/AnimNotifyState_AttackInputWindow.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Animation/AnimSequenceBase.h"
#include "Character/PdCharacterBase.h"
#include "Components/SkeletalMeshComponent.h"

namespace
{
	/** ì§€?•í•œ ?œê·¸??ê³µê²© ê´€??ê²Œì„?Œë ˆ???´ë²¤?¸ë? ?¡í„°???„ì†¡?©ë‹ˆ?? */
	void SendAttackGameplayEvent(USkeletalMeshComponent* MeshComp, const FGameplayTag& EventTag)
	{
		// =================================================================================================================
		// === ê¸°ë³¸ ? íš¨??ê²€??
		if (!IsValid(MeshComp) || !EventTag.IsValid())
		{
			return;
		}

		AActor* OwnerActor = MeshComp->GetOwner();
		if (!IsValid(OwnerActor))
		{
			return;
		}

		// =================================================================================================================
		// === AbilitySystemComponent ì¡´ì¬ ?¬ë? ?•ì¸

		if (!UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OwnerActor))
		{
			return;
		}

		// =================================================================================================================
		// === ê²Œì„?Œë ˆ???´ë²¤???„ì†¡

		FGameplayEventData Payload;
		Payload.EventTag = EventTag;
		Payload.Instigator = OwnerActor;
		Payload.Target = OwnerActor;
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor, EventTag, Payload);

		APdCharacterBase* Character = Cast<APdCharacterBase>(OwnerActor);
		if (Character && Character->IsLocallyControlled() && !Character->HasAuthority())
		{
			Character->ServerSendGameplayEventToSelf(Payload);
		}
	}
}

/** ?¸í‹°?Œì´ ?œì‘ ?œì ??ê³µê²© ?…ë ¥ ê°€???œì‘ ?´ë²¤?¸ë? ?„ì†¡?©ë‹ˆ?? */
void UAnimNotifyState_AttackInputWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	// ?¬ìš©?˜ì? ?ŠëŠ” ?¸ì ê²½ê³ ë¥?ë°©ì??©ë‹ˆ??
	static_cast<void>(Animation);
	static_cast<void>(TotalDuration);
	static_cast<void>(EventReference);

	// =================================================================================================================
	// === ?œì‘ ?´ë²¤???„ì†¡

	SendAttackGameplayEvent(MeshComp, StartEventTag);
}

/** ?¸í‹°?Œì´ ì¢…ë£Œ ?œì ??ê³µê²© ?…ë ¥ ê°€??ì¢…ë£Œ ?´ë²¤?¸ë? ?„ì†¡?©ë‹ˆ?? */
void UAnimNotifyState_AttackInputWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	// ?¬ìš©?˜ì? ?ŠëŠ” ?¸ì ê²½ê³ ë¥?ë°©ì??©ë‹ˆ??
	static_cast<void>(Animation);
	static_cast<void>(EventReference);

	// =================================================================================================================
	// === ì¢…ë£Œ ?´ë²¤???„ì†¡

	SendAttackGameplayEvent(MeshComp, EndEventTag);
}

/** ?ë””?°ì? ?”ë²„ê·¸ì—???œì‹œ???¸í‹°?Œì´ ?´ë¦„??ë°˜í™˜?©ë‹ˆ?? */
FString UAnimNotifyState_AttackInputWindow::GetNotifyName_Implementation() const
{
	// =================================================================================================================
	// === ?œì‘/ì¢…ë£Œ ?œê·¸ ?¬í•¨ ?¸í‹°?Œì´ ?´ë¦„ êµ¬ì„±

	return TEXT("AttackInputWindow");
}