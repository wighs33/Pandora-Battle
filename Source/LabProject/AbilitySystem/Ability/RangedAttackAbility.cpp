#include "AbilitySystem/Ability/RangedAttackAbility.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Character/PdCharacterBase.h"
#include "Common/LabGameplayTags.h"
#include "PlayerComponent/EquipmentComponent.h"
#include "Weapon/WeaponBase.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(RangedAttackAbility)

URangedAttackAbility::URangedAttackAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bRetriggerInstancedAbility = true;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;

	FGameplayTagContainer AbilityAssetTags;
	AbilityAssetTags.AddTag(LabGameplayTags::Action_RangedAttack);
	SetAssetTags(AbilityAssetTags);

	AttackTraceStartEventTag = FGameplayTag::RequestGameplayTag(TEXT("Notifier.Attack.ComboInputOpen"), false);
	AttackTraceEndEventTag = FGameplayTag::RequestGameplayTag(TEXT("Notifier.Attack.ComboInputClose"), false);
}

// State helpers
void URangedAttackAbility::CleanupAttackState()
{
	SetCurrentWeaponTraceEnabled(false);

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

void URangedAttackAbility::OnAttackTraceStart(FGameplayEventData Payload)
{
	static_cast<void>(Payload);
	SetCurrentWeaponTraceEnabled(true);
}

void URangedAttackAbility::OnAttackTraceEnd(FGameplayEventData Payload)
{
	static_cast<void>(Payload);
	SetCurrentWeaponTraceEnabled(false);
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

	if (AttackTraceStartEventTag.IsValid())
	{
		UAbilityTask_WaitGameplayEvent* AttackTraceStartTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this,
			AttackTraceStartEventTag,
			nullptr,
			false,
			true);
		if (ensure(AttackTraceStartTask))
		{
			AttackTraceStartTask->EventReceived.AddDynamic(this, &URangedAttackAbility::OnAttackTraceStart);
			AttackTraceStartTask->ReadyForActivation();
		}
	}

	if (AttackTraceEndEventTag.IsValid())
	{
		UAbilityTask_WaitGameplayEvent* AttackTraceEndTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this,
			AttackTraceEndEventTag,
			nullptr,
			false,
			true);
		if (ensure(AttackTraceEndTask))
		{
			AttackTraceEndTask->EventReceived.AddDynamic(this, &URangedAttackAbility::OnAttackTraceEnd);
			AttackTraceEndTask->ReadyForActivation();
		}
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

AWeaponBase* URangedAttackAbility::GetCurrentWeaponActor() const
{
	const APdCharacterBase* Character = GetPdCharacterFromActorInfo();
	const UEquipmentComponent* EquipmentComponent = Character ? Character->GetEquipmentComponent() : nullptr;
	return EquipmentComponent ? EquipmentComponent->GetCurrentWeaponActor() : nullptr;
}

void URangedAttackAbility::SetCurrentWeaponTraceEnabled(bool bEnabled) const
{
	AWeaponBase* CurrentWeapon = GetCurrentWeaponActor();
	if (!CurrentWeapon)
	{
		return;
	}

	CurrentWeapon->SetBeginOverlapEnabled(bEnabled);
}
