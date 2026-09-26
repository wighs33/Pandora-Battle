#include "Skill/Actions/SkillWeaponTrailAction.h"

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
#include "Weapon/MeleeWeapon.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkillWeaponTrailAction)

void USkillWeaponTrailAction::OnStart()
{
	const auto* ActorInfo = GetAbility()->GetCurrentActorInfo();

	TrailMontageTask = nullptr;
	TrailDurationTask = nullptr;
	TrailAttackTraceStartTask = nullptr;
	TrailAttackTraceEndTask = nullptr;
	bStartedWeaponTrail = false;

	USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
	if (!SkillDataAsset)
	{

		Finish(false);
		return;
	}


	const bool bHasTrailSystem = Settings.TrailNiagaraSystem != nullptr;
	const bool bUsesSlashHitTrace = Settings.bEnableSlashHitTrace;
	if (!bHasTrailSystem && !bUsesSlashHitTrace)
	{

		Finish(false);
		return;
	}

	AWeaponBase* CurrentWeapon = GetAbility()->GetCurrentWeaponActorFromAvatar();
	if (!CurrentWeapon)
	{

		Finish(false);
		return;
	}

	if (bHasTrailSystem && !GetAbility()->HasCurrentWeaponSkillTrail())
	{

		Finish(false);
		return;
	}

	if (!GetAbility()->CommitSkill())
	{
		Finish(false);
		return;
	}
	GetAbility()->StartDurationMovementLock();

	const FSkillGameplayEffectConfig AdditionalDamageConfig = SkillDataAsset->GetResolvedDamageConfig();
	const float AdditionalDamageMagnitude = AdditionalDamageConfig.GameplayEffectClass
		? GetAbility()->CalculateDamageMagnitude(AdditionalDamageConfig)
		: 0.0f;
	const FGameplayEffectSpecHandle DebuffEffectSpecHandle =
		GetAbility()->MakeConfiguredStatusEffectSpec(SkillDataAsset);

	AMeleeWeapon* MeleeWeapon = Cast<AMeleeWeapon>(CurrentWeapon);
	if (MeleeWeapon)
	{
		MeleeWeapon->ConfigureSkillSlash(
			Settings.SlashNiagaraSystem,
			Settings.SlashTransformOffset.GetScale3D(),
			Settings.SlashTransformOffset.GetLocation(),
			Settings.SlashSpawnSocketName,
			Settings.SlashTransformOffset.Rotator(),
			static_cast<float>(Settings.TrailEndZLengthMultiplier),
			bUsesSlashHitTrace,
			AdditionalDamageConfig.GameplayEffectClass,
			AdditionalDamageConfig.MagnitudeDataTag,
			AdditionalDamageMagnitude,
			FMath::Max(GetAbility()->GetAbilityLevel(), 1),
			GetAbility()->GetCurrentSourceObject(),
			DebuffEffectSpecHandle,
			SkillDataAsset->StatusEffectDataAsset.Get());
	}

	if (bHasTrailSystem && !GetAbility()->StartCurrentWeaponSkillTrail(Settings.TrailNiagaraSystem))
	{

		if (MeleeWeapon)
		{
			MeleeWeapon->ClearSkillSlash();
		}
		Finish(false);
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
	auto StartDurationTask = [this](const float RequestedDuration) -> bool
	{
		const float Duration = FMath::Max(RequestedDuration, 0.0f);
		if (Duration <= 0.0f)
		{

			Finish(true);
			return false;
		}

		TrailDurationTask = UAbilityTask_WaitDelay::WaitDelay(GetAbility(), Duration);
		if (!TrailDurationTask)
		{

			Finish(false);
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
	if (AMeleeWeapon* CurrentWeapon = Cast<AMeleeWeapon>(GetAbility()->GetCurrentWeaponActorFromAvatar()))
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
		Finish(true);
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
		Finish(true);
	}
}

void USkillWeaponTrailAction::HandleTrailDurationFinished()
{

	TrailDurationTask = nullptr;
	Finish();
}

void USkillWeaponTrailAction::HandleTrailAttackTraceStart(FGameplayEventData)
{
	if (AMeleeWeapon* CurrentWeapon = Cast<AMeleeWeapon>(GetAbility()->GetCurrentWeaponActorFromAvatar()))
	{

		CurrentWeapon->StartAttackTrace();
	}
}

void USkillWeaponTrailAction::HandleTrailAttackTraceEnd(FGameplayEventData)
{
	if (AMeleeWeapon* CurrentWeapon = Cast<AMeleeWeapon>(GetAbility()->GetCurrentWeaponActorFromAvatar()))
	{

		CurrentWeapon->StopAttackTrace();
	}
}
