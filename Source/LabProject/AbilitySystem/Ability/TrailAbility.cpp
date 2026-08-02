#include "AbilitySystem/Ability/TrailAbility.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Definition/AbilitySystem/SkillTypes.h"
#include "Animation/AnimMontage.h"
#include "Common/LabGameplayTags.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "NiagaraSystem.h"
#include "Weapon/WeaponBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TrailAbility)

namespace
{
	const FTrailSkillConfig* GetTrailConfig(const USkillDefinition* SkillDataAsset)
	{
		return SkillDataAsset && SkillDataAsset->SkillDataType == EPdSkillDataType::Trail
			? &SkillDataAsset->Trail
			: nullptr;
	}

	float GetPositiveDuration(const float Duration)
	{
		return FMath::Max(Duration, 0.0f);
	}
}

UTrailAbility::UTrailAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateNo;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UTrailAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	static_cast<void>(TriggerEventData);

	TrailMontageTask = nullptr;
	TrailDurationTask = nullptr;
	TrailAttackTraceStartTask = nullptr;
	TrailAttackTraceEndTask = nullptr;
	bStartedWeaponTrail = false;

	USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	if (!SkillDataAsset)
	{

		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (SkillDataAsset->SkillDataType != EPdSkillDataType::Trail)
	{

		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FTrailSkillConfig* TrailConfig = GetTrailConfig(SkillDataAsset);
	if (!TrailConfig)
	{

		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const bool bHasTrailSystem = TrailConfig->TrailSystem != nullptr;
	const bool bUsesSlashHitTrace = TrailConfig->bEnableSlashHitTrace;
	if (!bHasTrailSystem && !bUsesSlashHitTrace)
	{

		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AWeaponBase* CurrentWeapon = GetCurrentWeaponActorFromAvatar();
	if (!CurrentWeapon)
	{

		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (bHasTrailSystem && !HasCurrentWeaponSkillTrail())
	{

		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	StartDurationMovementLock();

	const FSkillGameplayEffectConfig AdditionalDamageConfig = SkillDataAsset->GetResolvedDamageConfig();
	const float AdditionalDamageMagnitude = AdditionalDamageConfig.GameplayEffectClass
		? CalculateSkillDamageMagnitude(AdditionalDamageConfig)
		: 0.0f;

	CurrentWeapon->ConfigureSkillSlash(
		TrailConfig->SlashSystem,
		TrailConfig->SlashScale,
		TrailConfig->SlashSpawnLocationOffset,
		TrailConfig->SlashSpawnSocketName,
		TrailConfig->SlashSpawnRotationOffset,
		static_cast<float>(TrailConfig->SlashAttackTraceEndMultiplier),
		bUsesSlashHitTrace,
		AdditionalDamageConfig.GameplayEffectClass,
		AdditionalDamageConfig.MagnitudeDataTag,
		AdditionalDamageMagnitude,
		FMath::Max(GetAbilityLevel(), 1),
		GetCurrentAbilitySpecSourceObject());


	if (bHasTrailSystem && !StartCurrentWeaponSkillTrail(TrailConfig->TrailSystem))
	{

		CurrentWeapon->ClearSkillSlash();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	bStartedWeaponTrail = bHasTrailSystem;
	SpawnConfiguredCharacterDecal();

	if (bUsesSlashHitTrace)
	{
		TrailAttackTraceStartTask = CreateWaitGameplayEventTask(LabGameplayTags::Notifier_Attack_ComboInputOpen);
		if (ensure(TrailAttackTraceStartTask))
		{
			TrailAttackTraceStartTask->EventReceived.AddDynamic(this, &ThisClass::HandleTrailAttackTraceStart);
			TrailAttackTraceStartTask->ReadyForActivation();
		}

		TrailAttackTraceEndTask = CreateWaitGameplayEventTask(LabGameplayTags::Notifier_Attack_ComboInputClose);
		if (ensure(TrailAttackTraceEndTask))
		{
			TrailAttackTraceEndTask->EventReceived.AddDynamic(this, &ThisClass::HandleTrailAttackTraceEnd);
			TrailAttackTraceEndTask->ReadyForActivation();
		}
	}

	bool bTrailDurationTimerStarted = false;
	auto StartDurationTask = [this, Handle, ActorInfo, ActivationInfo](const float RequestedDuration) -> bool
	{
		const float Duration = GetPositiveDuration(RequestedDuration);
		if (Duration <= 0.0f)
		{

			EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
			return false;
		}

		TrailDurationTask = UAbilityTask_WaitDelay::WaitDelay(this, Duration);
		if (!TrailDurationTask)
		{

			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return false;
		}

		TrailDurationTask->OnFinish.AddDynamic(this, &ThisClass::HandleTrailDurationFinished);
		TrailDurationTask->ReadyForActivation();


		return true;
	};

	const bool bUseConfiguredSkillDuration =
		SkillDataAsset->SkillType == EPdSkillType::Duration
		&& SkillDataAsset->Time.Duration > 0.0;
	if (bUseConfiguredSkillDuration)
	{
		bTrailDurationTimerStarted = StartDurationTask(
			static_cast<float>(SkillDataAsset->Time.Duration));
	}

	UAnimMontage* TrailMontage = TrailConfig->Animation.PrimaryMontage.Get();
	if (TrailMontage && ActorInfo && ActorInfo->GetAnimInstance())
	{
		TrailMontageTask = CreateDefaultMontageAndWaitTask(TrailMontage);
		if (TrailMontageTask)
		{
			TrailMontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleTrailMontageCompleted);
			TrailMontageTask->OnBlendOut.AddDynamic(this, &ThisClass::HandleTrailMontageCompleted);
			TrailMontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleTrailMontageInterrupted);
			TrailMontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleTrailMontageInterrupted);
			TrailMontageTask->ReadyForActivation();
			return;
		}
	}


	if (!bTrailDurationTimerStarted)
	{
		StartDurationTask(static_cast<float>(TrailConfig->Duration));
	}
}

void UTrailAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (TrailMontageTask)
	{
		TrailMontageTask->EndTask();
		TrailMontageTask = nullptr;
	}
	if (TrailDurationTask)
	{
		TrailDurationTask->EndTask();
		TrailDurationTask = nullptr;
	}
	if (TrailAttackTraceStartTask)
	{
		TrailAttackTraceStartTask->EndTask();
		TrailAttackTraceStartTask = nullptr;
	}
	if (TrailAttackTraceEndTask)
	{
		TrailAttackTraceEndTask->EndTask();
		TrailAttackTraceEndTask = nullptr;
	}

	if (bStartedWeaponTrail)
	{
		StopCurrentWeaponSkillTrail();
		bStartedWeaponTrail = false;
	}
	if (AWeaponBase* CurrentWeapon = GetCurrentWeaponActorFromAvatar())
	{
		CurrentWeapon->ClearSkillSlash();
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UTrailAbility::HandleTrailMontageCompleted()
{

	TrailMontageTask = nullptr;
	if (TrailDurationTask)
	{
		return;
	}

	if (IsEndAbilityValid(CurrentSpecHandle, CurrentActorInfo))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UTrailAbility::HandleTrailMontageInterrupted()
{

	TrailMontageTask = nullptr;
	if (TrailDurationTask)
	{
		return;
	}
	if (IsEndAbilityValid(CurrentSpecHandle, CurrentActorInfo))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UTrailAbility::HandleTrailDurationFinished()
{

	TrailDurationTask = nullptr;
	FinishAbilityFromDuration();
}

void UTrailAbility::HandleTrailAttackTraceStart(FGameplayEventData Payload)
{
	static_cast<void>(Payload);

	if (AWeaponBase* CurrentWeapon = GetCurrentWeaponActorFromAvatar())
	{

		CurrentWeapon->StartAttackTrace();
	}
}

void UTrailAbility::HandleTrailAttackTraceEnd(FGameplayEventData Payload)
{
	static_cast<void>(Payload);

	if (AWeaponBase* CurrentWeapon = GetCurrentWeaponActorFromAvatar())
	{

		CurrentWeapon->StopAttackTrace();
	}
}
