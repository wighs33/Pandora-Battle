#include "Skill/Actions/SkillWeaponTrailAction.h"
#include "AbilitySystem/Ability/SkillAbility.h"
#include "Character/CharacterBase.h"
#include "Component/Player/EquipmentComponent.h"

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

DEFINE_LOG_CATEGORY_STATIC(LogSkillWeaponTrailAction, Log, All);

void USkillWeaponTrailAction::OnStart()
{
	TrailMontageTask = nullptr;
	TrailDurationTask = nullptr;
	TrailAttackTraceStartTask = nullptr;
	TrailAttackTraceEndTask = nullptr;
	bStartedWeaponTrail = false;

	const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
	if (!SkillDataAsset)
	{
		UE_LOG(LogSkillWeaponTrailAction, Warning, TEXT("%s has no source skill definition, so the weapon trail cannot start."),
			*GetNameSafe(GetAbility()));
		Finish(false);
		return;
	}

	const bool bHasTrailSystem = Settings.TrailNiagaraSystem != nullptr;
	const bool bUsesSlashHitTrace = Settings.bEnableSlashHitTrace;
	AWeaponBase* CurrentWeapon = bHasTrailSystem || bUsesSlashHitTrace ? ResolveTrailWeapon(bHasTrailSystem) : nullptr;
	if (!CurrentWeapon || !GetAbility()->CommitSkill())
	{
		Finish(false);
		return;
	}
	GetAbility()->StartDurationMovementLock();
	TrailWeapon = CurrentWeapon;

	AMeleeWeapon* MeleeWeapon = Cast<AMeleeWeapon>(CurrentWeapon);
	PrepareSlash(MeleeWeapon, *SkillDataAsset);
	if (bHasTrailSystem && !CurrentWeapon->StartSkillWeaponTrail(Settings.TrailNiagaraSystem))
	{
		if (MeleeWeapon)
		{
			MeleeWeapon->ClearSkillSlash();
		}
		Finish(false);
		return;
	}
	bStartedWeaponTrail = bHasTrailSystem;
	GetAbility()->SpawnConfiguredCharacterDecal();

	if (bUsesSlashHitTrace)
	{
		ListenForSlashHitWindow();
	}
	WaitForTrailEnd(*SkillDataAsset);
}

AWeaponBase* USkillWeaponTrailAction::ResolveTrailWeapon(const bool bNeedsTrailComponent) const
{
	const ACharacterBase* Character = GetAbility()->GetPdCharacterFromActorInfo();
	const UEquipmentComponent* Equipment = Character ? Character->GetEquipmentComponent() : nullptr;
	AWeaponBase* CurrentWeapon = Equipment ? Equipment->GetCurrentWeaponActor() : nullptr;
	return CurrentWeapon && (!bNeedsTrailComponent || CurrentWeapon->HasSkillWeaponTrailComponent()) ? CurrentWeapon : nullptr;
}

void USkillWeaponTrailAction::PrepareSlash(AMeleeWeapon* MeleeWeapon, const USkillDefinition& SkillDataAsset)
{
	const FSkillGameplayEffectConfig AdditionalDamageConfig = SkillDataAsset.GetResolvedDamageConfig();
	const float AdditionalDamageMagnitude = AdditionalDamageConfig.GameplayEffectClass
		? GetAbility()->CalculateDamageMagnitude(AdditionalDamageConfig)
		: 0.0f;
	const FGameplayEffectSpecHandle DebuffEffectSpecHandle = GetAbility()->MakeConfiguredStatusEffectSpec(&SkillDataAsset);
	if (!MeleeWeapon)
	{
		return;
	}

	MeleeWeapon->ConfigureSkillSlash(Settings.SlashNiagaraSystem, Settings.SlashTransformOffset.GetScale3D(),
		Settings.SlashTransformOffset.GetLocation(), Settings.SlashSpawnSocketName, Settings.SlashTransformOffset.Rotator(),
		static_cast<float>(Settings.TrailEndZLengthMultiplier), Settings.bEnableSlashHitTrace, AdditionalDamageConfig.GameplayEffectClass,
		AdditionalDamageConfig.MagnitudeDataTag, AdditionalDamageMagnitude, FMath::Max(GetAbility()->GetAbilityLevel(), 1),
		GetAbility()->GetCurrentSourceObject(), DebuffEffectSpecHandle, SkillDataAsset.StatusEffectDataAsset.Get());
}

void USkillWeaponTrailAction::ListenForSlashHitWindow()
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

void USkillWeaponTrailAction::WaitForTrailEnd(const USkillDefinition& SkillDataAsset)
{
	const auto* ActorInfo = GetAbility()->GetCurrentActorInfo();
	UAnimMontage* TrailMontage = SkillDataAsset.Animation.PrimaryMontage.Get();
	if (TrailMontage && ActorInfo && ActorInfo->GetAnimInstance())
	{
		TrailMontageTask = GetAbility()->CreateDefaultMontageAndWaitTask(TrailMontage);
		if (TrailMontageTask)
		{
			TrailMontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleTrailMontageFinished);
			TrailMontageTask->OnBlendOut.AddDynamic(this, &ThisClass::HandleTrailMontageFinished);
			TrailMontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleTrailMontageFinished);
			TrailMontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleTrailMontageFinished);
			TrailMontageTask->ReadyForActivation();
			return;
		}
	}

	// Duration은 액션 시작 시 타이머를 다시 만들지 않고 스킬 종료까지 유지한다.
	if (GetAbility()->HasDurationDeadline())
	{
		return;
	}
	const float Duration = FMath::Max(static_cast<float>(SkillDataAsset.Time.Duration), 0.0f);
	if (Duration <= 0.0f)
	{
		Finish(true);
		return;
	}
	TrailDurationTask = UAbilityTask_WaitDelay::WaitDelay(GetAbility(), Duration);
	if (!TrailDurationTask)
	{
		Finish(false);
		return;
	}
	TrailDurationTask->OnFinish.AddDynamic(this, &ThisClass::HandleTrailDurationFinished);
	TrailDurationTask->ReadyForActivation();
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
		if (AWeaponBase* Weapon = TrailWeapon.Get())
		{
			Weapon->StopSkillWeaponTrail();
		}
		bStartedWeaponTrail = false;
	}
	if (AMeleeWeapon* CurrentWeapon = Cast<AMeleeWeapon>(TrailWeapon.Get()))
	{
		CurrentWeapon->ClearSkillSlash();
	}
	TrailWeapon.Reset();
}

void USkillWeaponTrailAction::HandleTrailMontageFinished()
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
	if (AMeleeWeapon* CurrentWeapon = Cast<AMeleeWeapon>(TrailWeapon.Get()))
	{
		CurrentWeapon->StartAttackTrace();
	}
}

void USkillWeaponTrailAction::HandleTrailAttackTraceEnd(FGameplayEventData)
{
	if (AMeleeWeapon* CurrentWeapon = Cast<AMeleeWeapon>(TrailWeapon.Get()))
	{
		CurrentWeapon->StopAttackTrace();
	}
}
