#include "Skill/Actions/SkillShieldAction.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimMontage.h"
#include "Common/LabGameplayTags.h"
#include "GameplayEffect.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkillShieldAction)

void USkillShieldAction::OnStart()
{
	const auto Handle = GetAbility()->GetCurrentAbilitySpecHandle();
	const auto* ActorInfo = GetAbility()->GetCurrentActorInfo();
	const auto ActivationInfo = GetAbility()->GetCurrentActivationInfo();
	const auto* TriggerEventData = &GetContext().EventData;
	static_cast<void>(TriggerEventData);

	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid())
	{
		Finish(!(true));
		return;
	}

	bShieldApplied = false;
	const UAnimMontage* ResolvedShieldMontage = GetResolvedShieldMontage();
	StartWaitMontageTriggerTask();

	if (!ResolvedShieldMontage)
	{

		ApplyShieldFromMontageTrigger();
		if (IsRunning())
		{
			Finish(!(false));
		}
		return;
	}

	if (!StartShieldMontageTask())
	{
		Finish(!(true));
	}
}

void USkillShieldAction::OnStop()
{
	CleanupShieldTasks();
}

UAnimMontage* USkillShieldAction::GetResolvedShieldMontage() const
{
	const USkillDefinition* ShieldConfig = GetAbility()->GetSourceSkillDataAsset();
	return ShieldConfig && ShieldConfig->Animation.PrimaryMontage
		? ShieldConfig->Animation.PrimaryMontage.Get()
		: nullptr;
}

TSubclassOf<UGameplayEffect> USkillShieldAction::GetResolvedShieldGameplayEffectClass() const
{
	const USkillDefinition* ShieldConfig = GetAbility()->GetSourceSkillDataAsset();
	return ShieldConfig && ShieldConfig->GameplayEffect.GameplayEffectClass
		? ShieldConfig->GameplayEffect.GameplayEffectClass
		: nullptr;
}

FGameplayTag USkillShieldAction::GetResolvedMontageTriggerEventTag() const
{
	const USkillDefinition* ShieldConfig = GetAbility()->GetSourceSkillDataAsset();
	return ShieldConfig && ShieldConfig->Animation.PrimaryEventTag.IsValid()
		? ShieldConfig->Animation.PrimaryEventTag
		: LabGameplayTags::Event_Montage_Trigger;
}

void USkillShieldAction::StartWaitMontageTriggerTask()
{
	const FGameplayTag ResolvedMontageTriggerEventTag = GetResolvedMontageTriggerEventTag();
	if (!ResolvedMontageTriggerEventTag.IsValid())
	{

		return;
	}

	WaitMontageTriggerTask = GetAbility()->CreateWaitGameplayEventTask(ResolvedMontageTriggerEventTag);
	if (!WaitMontageTriggerTask)
	{

		return;
	}

	WaitMontageTriggerTask->EventReceived.AddDynamic(this, &ThisClass::HandleMontageTriggerEvent);
	WaitMontageTriggerTask->ReadyForActivation();
}

bool USkillShieldAction::StartShieldMontageTask()
{
	UAnimMontage* ResolvedShieldMontage = GetResolvedShieldMontage();
	ShieldMontageTask = GetAbility()->CreateDefaultMontageAndWaitTask(ResolvedShieldMontage);
	if (!ShieldMontageTask)
	{

		return false;
	}

	ShieldMontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleShieldMontageFinished);
	ShieldMontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleShieldMontageFinished);
	ShieldMontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleShieldMontageFinished);
	ShieldMontageTask->ReadyForActivation();
	return true;
}

void USkillShieldAction::ApplyShieldFromMontageTrigger()
{
	if (bShieldApplied || !(IsRunning() && GetAbility()->CanRunActions()))
	{
		return;
	}
	const TSubclassOf<UGameplayEffect> ResolvedShieldGameplayEffectClass =
		GetResolvedShieldGameplayEffectClass();
	if (!ResolvedShieldGameplayEffectClass)
	{
		Finish(false);
		return;
	}
	bShieldApplied = true;

	if (!GetAbility()->CommitSkill())
	{

		Finish();
		return;
	}

	GetAbility()->SpawnConfiguredCharacterDecal();
	const FActiveGameplayEffectHandle EffectHandle =
		GetAbility()->BP_ApplyGameplayEffectToOwner(ResolvedShieldGameplayEffectClass, FMath::Max(GetAbility()->GetAbilityLevel(), 1), 1);
	if (GetAbility()->K2_HasAuthority() && !EffectHandle.WasSuccessfullyApplied())
	{
		Finish(false);
	}
}

void USkillShieldAction::CleanupShieldTasks()
{
	if (ShieldMontageTask)
	{
		ShieldMontageTask->EndTask();
		ShieldMontageTask = nullptr;
	}
	if (WaitMontageTriggerTask)
	{
		WaitMontageTriggerTask->EndTask();
		WaitMontageTriggerTask = nullptr;
	}
}

void USkillShieldAction::HandleShieldMontageFinished()
{
	ShieldMontageTask = nullptr;
	if (!bShieldApplied)
	{
		ApplyShieldFromMontageTrigger();
	}
	if (IsRunning())
	{
		Finish();
	}
}

void USkillShieldAction::HandleMontageTriggerEvent(FGameplayEventData Payload)
{

ApplyShieldFromMontageTrigger();
}
