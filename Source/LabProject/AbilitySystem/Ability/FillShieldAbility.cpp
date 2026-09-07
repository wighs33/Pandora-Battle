#include "AbilitySystem/Ability/FillShieldAbility.h"

#include "Component/AbilitySystem/Ability/AbilityPresentationRuntime.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimMontage.h"
#include "Common/LabGameplayTags.h"
#include "GameplayEffect.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(FillShieldAbility)

UFillShieldAbility::UFillShieldAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer AbilityAssetTags;
	AbilityAssetTags.AddTag(LabGameplayTags::GameplayAbility_Defensive);
	AbilityAssetTags.AddTag(LabGameplayTags::GameplayAbility_Defensive_FillShield);
	SetAssetTags(AbilityAssetTags);

	ActivationOwnedTags.AddTag(LabGameplayTags::GameplayAbility_Defensive_FillShield_Active);
}

void UFillShieldAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	static_cast<void>(TriggerEventData);

	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bFillShieldApplied = false;
	if (!GetConfiguredFillShieldMontage()
		|| !GetConfiguredFillShieldGameplayEffectClass()
		|| !GetConfiguredMontageTriggerEventTag().IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	StartWaitMontageTriggerTask();

	if (!StartFillShieldMontageTask())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
}

void UFillShieldAbility::OnAbilityEnding()
{
	Super::OnAbilityEnding();
	CleanupFillShieldTasks();
}

const FShieldSkillConfig* UFillShieldAbility::GetFillShieldSkillConfig() const
{
	const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	return SkillDataAsset ? SkillDataAsset->GetDefensiveSkillConfig() : nullptr;
}

UAnimMontage* UFillShieldAbility::GetConfiguredFillShieldMontage() const
{
	const FShieldSkillConfig* FillShieldConfig = GetFillShieldSkillConfig();
	return FillShieldConfig
		? FillShieldConfig->Animation.PrimaryMontage.Get()
		: nullptr;
}

TSubclassOf<UGameplayEffect> UFillShieldAbility::GetConfiguredFillShieldGameplayEffectClass() const
{
	const FShieldSkillConfig* FillShieldConfig = GetFillShieldSkillConfig();
	return FillShieldConfig
		? FillShieldConfig->GameplayEffectClass
		: nullptr;
}

FGameplayTag UFillShieldAbility::GetConfiguredMontageTriggerEventTag() const
{
	const FShieldSkillConfig* FillShieldConfig = GetFillShieldSkillConfig();
	return FillShieldConfig
		? FillShieldConfig->Animation.PrimaryEventTag
		: FGameplayTag();
}

void UFillShieldAbility::StartWaitMontageTriggerTask()
{
	const FGameplayTag MontageTriggerEventTag = GetConfiguredMontageTriggerEventTag();
	if (!MontageTriggerEventTag.IsValid())
	{
		return;
	}

	WaitMontageTriggerTask = CreateWaitGameplayEventTask(MontageTriggerEventTag);
	if (!WaitMontageTriggerTask)
	{
		return;
	}

	WaitMontageTriggerTask->EventReceived.AddDynamic(this, &ThisClass::HandleMontageTriggerEvent);
	WaitMontageTriggerTask->ReadyForActivation();
}

bool UFillShieldAbility::StartFillShieldMontageTask()
{
	FillShieldMontageTask = CreateDefaultMontageAndWaitTask(
		GetConfiguredFillShieldMontage());
	if (!FillShieldMontageTask)
	{
		return false;
	}

	FillShieldMontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleFillShieldMontageFinished);
	FillShieldMontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleFillShieldMontageFinished);
	FillShieldMontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleFillShieldMontageFinished);
	FillShieldMontageTask->ReadyForActivation();
	return true;
}

void UFillShieldAbility::ApplyFillShieldFromMontageTrigger()
{
	if (bFillShieldApplied || !CanExecuteSkillPayload())
	{
		return;
	}
	const TSubclassOf<UGameplayEffect> FillShieldGameplayEffectClass =
		GetConfiguredFillShieldGameplayEffectClass();
	if (!FillShieldGameplayEffectClass)
	{
		K2_CancelAbility();
		return;
	}
	bFillShieldApplied = true;

	if (!CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
	{

		K2_EndAbility();
		return;
	}

	GetPresentationRuntime().SpawnConfiguredCharacterDecal(*this);
	const FActiveGameplayEffectHandle EffectHandle =
		BP_ApplyGameplayEffectToOwner(FillShieldGameplayEffectClass, FMath::Max(GetAbilityLevel(), 1), 1);
	if (K2_HasAuthority() && !EffectHandle.WasSuccessfullyApplied())
	{
		CancelAbilityForSkillExecutionFailure();
	}
}

void UFillShieldAbility::CleanupFillShieldTasks()
{
	if (FillShieldMontageTask)
	{
		FillShieldMontageTask->EndTask();
		FillShieldMontageTask = nullptr;
	}
	if (WaitMontageTriggerTask)
	{
		WaitMontageTriggerTask->EndTask();
		WaitMontageTriggerTask = nullptr;
	}
}

void UFillShieldAbility::HandleFillShieldMontageFinished()
{
	FillShieldMontageTask = nullptr;
	if (!bFillShieldApplied)
	{
		ApplyFillShieldFromMontageTrigger();
	}
	if (IsEndAbilityValid(CurrentSpecHandle, CurrentActorInfo))
	{
		K2_EndAbility();
	}
}

void UFillShieldAbility::HandleMontageTriggerEvent(FGameplayEventData Payload)
{

ApplyFillShieldFromMontageTrigger();
}
