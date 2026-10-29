#include "Animation/AnimNotifyState_AttackDamageWindow.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Animation/AnimSequenceBase.h"
#include "Common/PdGameplayTags.h"
#include "Components/SkeletalMeshComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNotifyState_AttackDamageWindow)

namespace
{
void SendAttackDamageWindowEvent(USkeletalMeshComponent* MeshComp, const FGameplayTag& EventTag)
{
	if (!IsValid(MeshComp) || !EventTag.IsValid())
	{
		return;
	}

	AActor* OwnerActor = MeshComp->GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority()
		|| !UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OwnerActor))
	{
		return;
	}

	FGameplayEventData Payload;
	Payload.EventTag = EventTag;
	Payload.Instigator = OwnerActor;
	Payload.Target = OwnerActor;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor, EventTag, Payload);
}
}

UAnimNotifyState_AttackDamageWindow::UAnimNotifyState_AttackDamageWindow()
{
	StartEventTag = PdGameplayTags::Notifier_Attack_DamageWindowOpen;
	EndEventTag = PdGameplayTags::Notifier_Attack_DamageWindowClose;
}

void UAnimNotifyState_AttackDamageWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	static_cast<void>(Animation);
	static_cast<void>(TotalDuration);
	static_cast<void>(EventReference);

	const FGameplayTag EventTag = StartEventTag.IsValid()
		? StartEventTag
		: PdGameplayTags::Notifier_Attack_DamageWindowOpen;
	SendAttackDamageWindowEvent(MeshComp, EventTag);
}

void UAnimNotifyState_AttackDamageWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	static_cast<void>(Animation);
	static_cast<void>(EventReference);

	const FGameplayTag EventTag = EndEventTag.IsValid()
		? EndEventTag
		: PdGameplayTags::Notifier_Attack_DamageWindowClose;
	SendAttackDamageWindowEvent(MeshComp, EventTag);
}

FString UAnimNotifyState_AttackDamageWindow::GetNotifyName_Implementation() const
{
	return TEXT("Attack Damage Window");
}
