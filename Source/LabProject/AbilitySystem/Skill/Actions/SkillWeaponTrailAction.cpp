#include "AbilitySystem/Skill/Actions/SkillWeaponTrailAction.h"

#include "Component/AbilitySystem/Ability/AbilityPresentationManager.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Definition/AbilitySystem/SkillDefinition.h"
#include "Definition/AbilitySystem/StatusEffectDefinition.h"
#include "Animation/AnimMontage.h"
#include "Common/LabGameplayTags.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "NiagaraSystem.h"
#include "Weapon/WeaponBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkillWeaponTrailAction)

namespace
{
float GetPositiveDuration(const float Duration)
	{
		return FMath::Max(Duration, 0.0f);
	}
}

void USkillWeaponTrailAction::OnStart()
{
	const auto Handle = GetAbility()->GetCurrentAbilitySpecHandle();
	const auto* ActorInfo = GetAbility()->GetCurrentActorInfo();
	const auto ActivationInfo = GetAbility()->GetCurrentActivationInfo();
	const auto* TriggerEventData = &GetContext().EventData;
	static_cast<void>(TriggerEventData);

	TrailMontageTask = nullptr;
	TrailDurationTask = nullptr;
	TrailAttackTraceStartTask = nullptr;
	TrailAttackTraceEndTask = nullptr;
	bStartedWeaponTrail = false;

	USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
	if (!SkillDataAsset)
	{

		Finish(!(true));
		return;
	}


	const FSkillSwordTrailSettings* TrailConfig = &Settings;
	if (!TrailConfig)
	{

		Finish(!(true));
		return;
	}

	const bool bHasTrailSystem = TrailConfig->TrailNiagaraSystem != nullptr;
	const bool bUsesSlashHitTrace = TrailConfig->bEnableSlashHitTrace;
	if (!bHasTrailSystem && !bUsesSlashHitTrace)
	{

		Finish(!(true));
		return;
	}

	AWeaponBase* CurrentWeapon = GetAbility()->GetCurrentWeaponActorFromAvatar();
	if (!CurrentWeapon)
	{

		Finish(!(true));
		return;
	}

	if (bHasTrailSystem && !GetAbility()->HasCurrentWeaponSkillTrail())
	{

		Finish(!(true));
		return;
	}

	if (!GetAbility()->CommitSkill())
	{
		Finish(!(true));
		return;
	}
	GetAbility()->StartDurationMovementLock();

	const FSkillGameplayEffectConfig AdditionalDamageConfig = SkillDataAsset->GetResolvedDamageConfig();
	const float AdditionalDamageMagnitude = AdditionalDamageConfig.GameplayEffectClass
		? GetAbility()->CalculateDamageMagnitude(AdditionalDamageConfig)
		: 0.0f;
	const FGameplayEffectSpecHandle DebuffEffectSpecHandle =
		GetAbility()->MakeConfiguredStatusEffectSpec(SkillDataAsset);

	CurrentWeapon->ConfigureSkillSlash(
		TrailConfig->SlashNiagaraSystem,
		TrailConfig->SlashTransformOffset.GetScale3D(),
		TrailConfig->SlashTransformOffset.GetLocation(),
		TrailConfig->SlashSpawnSocketName,
		TrailConfig->SlashTransformOffset.Rotator(),
		static_cast<float>(TrailConfig->TrailEndZLengthMultiplier),
		bUsesSlashHitTrace,
		AdditionalDamageConfig.GameplayEffectClass,
		AdditionalDamageConfig.MagnitudeDataTag,
		AdditionalDamageMagnitude,
		FMath::Max(GetAbility()->GetAbilityLevel(), 1),
		GetAbility()->GetCurrentSourceObject(),
		DebuffEffectSpecHandle,
		SkillDataAsset->StatusEffectDataAsset.Get());

	if (bHasTrailSystem && !GetAbility()->StartCurrentWeaponSkillTrail(TrailConfig->TrailNiagaraSystem))
	{

		CurrentWeapon->ClearSkillSlash();
		Finish(!(true));
		return;
	}
	bStartedWeaponTrail = bHasTrailSystem;
	GetAbility()->GetPresentationManager().SpawnConfiguredCharacterDecal(*GetAbility());

	if (bUsesSlashHitTrace)
	{
		TrailAttackTraceStartTask = GetAbility()->CreateWaitGameplayEventTask(LabGameplayTags::Notifier_Attack_ComboInputOpen);
		if (ensure(TrailAttackTraceStartTask))
		{
			TrailAttackTraceStartTask->EventReceived.AddDynamic(this, &ThisClass::HandleTrailAttackTraceStart);
			TrailAttackTraceStartTask->ReadyForActivation();
		}

		TrailAttackTraceEndTask = GetAbility()->CreateWaitGameplayEventTask(LabGameplayTags::Notifier_Attack_ComboInputClose);
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

			Finish(!(false));
			return false;
		}

		TrailDurationTask = UAbilityTask_WaitDelay::WaitDelay(GetAbility(), Duration);
		if (!TrailDurationTask)
		{

			Finish(!(true));
			return false;
		}

		TrailDurationTask->OnFinish.AddDynamic(this, &ThisClass::HandleTrailDurationFinished);
		TrailDurationTask->ReadyForActivation();

		return true;
	};

	// Duration은 액션 시작 시 타이머를 다시 만들지 않고 스킬 종료까지 유지한다.
	bTrailDurationTimerStarted = GetAbility()->HasDurationDeadline();

	UAnimMontage* TrailMontage = SkillDataAsset->Animation.PrimaryMontage.Get();
	if (TrailMontage && ActorInfo && ActorInfo->GetAnimInstance())
	{
		TrailMontageTask = GetAbility()->CreateDefaultMontageAndWaitTask(TrailMontage);
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
		StartDurationTask(static_cast<float>(SkillDataAsset->Time.Duration));
	}
}

void USkillWeaponTrailAction::OnStop()
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
		GetAbility()->StopCurrentWeaponSkillTrail();
		bStartedWeaponTrail = false;
	}
	if (AWeaponBase* CurrentWeapon = GetAbility()->GetCurrentWeaponActorFromAvatar())
	{
		CurrentWeapon->ClearSkillSlash();
	}
}

void USkillWeaponTrailAction::HandleTrailMontageCompleted()
{

	TrailMontageTask = nullptr;
	if (GetAbility()->HasDurationDeadline() || TrailDurationTask)
	{
		return;
	}

	if (IsRunning())
	{
		Finish(!(false));
	}
}

void USkillWeaponTrailAction::HandleTrailMontageInterrupted()
{

	TrailMontageTask = nullptr;
	if (GetAbility()->HasDurationDeadline() || TrailDurationTask)
	{
		return;
	}
	if (IsRunning())
	{
		Finish(!(false));
	}
}

void USkillWeaponTrailAction::HandleTrailDurationFinished()
{

	TrailDurationTask = nullptr;
	Finish();
}

void USkillWeaponTrailAction::HandleTrailAttackTraceStart(FGameplayEventData Payload)
{
	static_cast<void>(Payload);

	if (AWeaponBase* CurrentWeapon = GetAbility()->GetCurrentWeaponActorFromAvatar())
	{

		CurrentWeapon->StartAttackTrace();
	}
}

void USkillWeaponTrailAction::HandleTrailAttackTraceEnd(FGameplayEventData Payload)
{
	static_cast<void>(Payload);

	if (AWeaponBase* CurrentWeapon = GetAbility()->GetCurrentWeaponActorFromAvatar())
	{

		CurrentWeapon->StopAttackTrace();
	}
}
