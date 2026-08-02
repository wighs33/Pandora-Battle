#include "Animation/AnimNotifyState_AttackInputWindow.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Animation/AnimSequenceBase.h"
#include "Character/CharacterBase.h"
#include "Common/LabGameplayTags.h"
#include "Components/SkeletalMeshComponent.h"

namespace
{
	void SendAttackGameplayEvent(USkeletalMeshComponent* MeshComp, const FGameplayTag& EventTag)
	{
		// =================================================================================================================
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

		UAbilitySystemComponent* AbilitySystemComponent =
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OwnerActor);
		if (!AbilitySystemComponent)
		{
			return;
		}

		// =================================================================================================================

		FGameplayEventData Payload;
		Payload.EventTag = EventTag;
		Payload.Instigator = OwnerActor;
		Payload.Target = OwnerActor;
		ACharacterBase* Character = Cast<ACharacterBase>(OwnerActor);
		const bool bIsAuthoritativeNotify = OwnerActor->HasAuthority();
		const bool bIsOwningClientPresentation =
			Character && Character->IsLocallyControlled() && !bIsAuthoritativeNotify;
		if (!bIsAuthoritativeNotify && !bIsOwningClientPresentation)
		{
			return;
		}

		if (bIsAuthoritativeNotify)
		{
			// Combo authority follows the server's montage and active ability tasks.
			AbilitySystemComponent->HandleGameplayEvent(EventTag, &Payload);
			return;
		}

		// The owning client's predicted notify may update an already-active local
		// presentation/input task, but it cannot activate an ability or reach the server.
		if (FGameplayEventMulticastDelegate* EventDelegate =
			AbilitySystemComponent->GenericGameplayEventCallbacks.Find(EventTag))
		{
			FGameplayEventMulticastDelegate DelegateCopy = *EventDelegate;
			DelegateCopy.Broadcast(&Payload);
		}
	}
}

UAnimNotifyState_AttackInputWindow::UAnimNotifyState_AttackInputWindow()
{
	StartEventTag = LabGameplayTags::Notifier_Attack_ComboInputOpen;
	EndEventTag = LabGameplayTags::Notifier_Attack_ComboInputClose;
}

void UAnimNotifyState_AttackInputWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	static_cast<void>(Animation);
	static_cast<void>(TotalDuration);
	static_cast<void>(EventReference);

	// =================================================================================================================

	const FGameplayTag EventTag = StartEventTag.IsValid() ? StartEventTag : LabGameplayTags::Notifier_Attack_ComboInputOpen;
	SendAttackGameplayEvent(MeshComp, EventTag);
}

void UAnimNotifyState_AttackInputWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	static_cast<void>(Animation);
	static_cast<void>(EventReference);

	// =================================================================================================================

	const FGameplayTag EventTag = EndEventTag.IsValid() ? EndEventTag : LabGameplayTags::Notifier_Attack_ComboInputClose;
	SendAttackGameplayEvent(MeshComp, EventTag);
}

FString UAnimNotifyState_AttackInputWindow::GetNotifyName_Implementation() const
{
	// =================================================================================================================

	return TEXT("Attack Combo Input Window");
}
