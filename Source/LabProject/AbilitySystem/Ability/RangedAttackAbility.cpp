#include "AbilitySystem/Ability/RangedAttackAbility.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Character/PdCharacterBase.h"
#include "PlayerComponent/EquipmentComponent.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(RangedAttackAbility)

URangedAttackAbility::URangedAttackAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bRetriggerInstancedAbility = true;
}

// State helpers
void URangedAttackAbility::CleanupAttackState()
{
	if (AttackingEffectClass && HasAuthority(&CurrentActivationInfo))
	{
		RemoveGameplayEffect(AttackingEffectClass);
	}
}

// Timing callbacks
void URangedAttackAbility::OnAttackMontageCompleted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void URangedAttackAbility::OnAttackMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void URangedAttackAbility::OnAttackMontageCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

// Ability flow
void URangedAttackAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	CleanupAttackState();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void URangedAttackAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	static_cast<void>(TriggerEventData);

	APdCharacterBase* Character = GetPdCharacterFromActorInfo();
	UEquipmentComponent* EquipmentComponent = Character ? Character->GetEquipmentComponent() : nullptr;
	if (!ensure(EquipmentComponent))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	FAttackData AttackData;
	if (!ensure(EquipmentComponent->GetAttackData(AttackData)))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	if (!ensure(CommitAbility(Handle, ActorInfo, ActivationInfo)))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		AttackData.AttackMontage,
		1.f,
		NAME_None,
		false,
		1.f,
		0.f,
		false);
	if (!ensure(MontageTask))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	MontageTask->OnCompleted.AddDynamic(this, &URangedAttackAbility::OnAttackMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &URangedAttackAbility::OnAttackMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &URangedAttackAbility::OnAttackMontageCancelled);

	if (AttackingEffectClass && !HasActiveGameplayEffect(AttackingEffectClass))
	{
		ApplyGameplayEffect(AttackingEffectClass, 1.f, 1);
	}

	MontageTask->ReadyForActivation();
}
