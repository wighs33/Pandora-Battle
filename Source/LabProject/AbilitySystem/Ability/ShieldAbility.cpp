#include "AbilitySystem/Ability/ShieldAbility.h"

#include "Component/AbilitySystem/Ability/AbilityPresentationRuntime.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimMontage.h"
#include "Common/LabGameplayTags.h"
#include "GameplayEffect.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ShieldAbility)

UShieldAbility::UShieldAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	MontageTriggerEventTag = LabGameplayTags::Event_Montage_Trigger;

	FGameplayTagContainer AbilityAssetTags;
	AbilityAssetTags.AddTag(LabGameplayTags::GameplayAbility_Defensive);
	AbilityAssetTags.AddTag(LabGameplayTags::GameplayAbility_Defensive_Shield);
	AbilityAssetTags.AddTag(LabGameplayTags::GameplayAbility_Defensive_ShieldBubble);
	SetAssetTags(AbilityAssetTags);

	ActivationOwnedTags.AddTag(LabGameplayTags::GameplayAbility_Defensive_Shield_Active);
	ActivationBlockedTags.AddTag(LabGameplayTags::State_DefenseField_Invulnerable);
}

void UShieldAbility::ActivateAbility(
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

	bShieldApplied = false;
	const UAnimMontage* ResolvedShieldMontage = GetResolvedShieldMontage();
	StartWaitMontageTriggerTask();

	if (!ResolvedShieldMontage)
	{

		ApplyShieldFromMontageTrigger();
		if (IsEndAbilityValid(Handle, ActorInfo))
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		}
		return;
	}

	if (!StartShieldMontageTask())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
}

void UShieldAbility::OnAbilityEnding()
{
	Super::OnAbilityEnding();
	CleanupShieldTasks();
}

const FShieldSkillConfig* UShieldAbility::GetShieldSkillConfig() const
{
	const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	return SkillDataAsset ? SkillDataAsset->GetDefensiveSkillConfig() : nullptr;
}

UAnimMontage* UShieldAbility::GetResolvedShieldMontage() const
{
	const FShieldSkillConfig* ShieldConfig = GetShieldSkillConfig();
	return ShieldConfig && ShieldConfig->Animation.PrimaryMontage
		? ShieldConfig->Animation.PrimaryMontage.Get()
		: ShieldMontage.Get();
}

TSubclassOf<UGameplayEffect> UShieldAbility::GetResolvedShieldGameplayEffectClass() const
{
	const FShieldSkillConfig* ShieldConfig = GetShieldSkillConfig();
	return ShieldConfig && ShieldConfig->GameplayEffectClass
		? ShieldConfig->GameplayEffectClass
		: ShieldGameplayEffectClass;
}

FGameplayTag UShieldAbility::GetResolvedMontageTriggerEventTag() const
{
	const FShieldSkillConfig* ShieldConfig = GetShieldSkillConfig();
	return ShieldConfig && ShieldConfig->Animation.PrimaryEventTag.IsValid()
		? ShieldConfig->Animation.PrimaryEventTag
		: MontageTriggerEventTag;
}

void UShieldAbility::StartWaitMontageTriggerTask()
{
	const FGameplayTag ResolvedMontageTriggerEventTag = GetResolvedMontageTriggerEventTag();
	if (!ResolvedMontageTriggerEventTag.IsValid())
	{

		return;
	}

	WaitMontageTriggerTask = CreateWaitGameplayEventTask(ResolvedMontageTriggerEventTag);
	if (!WaitMontageTriggerTask)
	{

		return;
	}

	WaitMontageTriggerTask->EventReceived.AddDynamic(this, &ThisClass::HandleMontageTriggerEvent);
	WaitMontageTriggerTask->ReadyForActivation();
}

bool UShieldAbility::StartShieldMontageTask()
{
	UAnimMontage* ResolvedShieldMontage = GetResolvedShieldMontage();
	ShieldMontageTask = CreateDefaultMontageAndWaitTask(ResolvedShieldMontage);
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

void UShieldAbility::ApplyShieldFromMontageTrigger()
{
	if (bShieldApplied || !CanExecuteSkillPayload())
	{
		return;
	}
	const TSubclassOf<UGameplayEffect> ResolvedShieldGameplayEffectClass =
		GetResolvedShieldGameplayEffectClass();
	if (!ResolvedShieldGameplayEffectClass)
	{
		K2_CancelAbility();
		return;
	}
	bShieldApplied = true;

	if (!CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
	{

		K2_EndAbility();
		return;
	}

	GetPresentationRuntime().SpawnConfiguredCharacterDecal(*this);
	const FActiveGameplayEffectHandle EffectHandle =
		BP_ApplyGameplayEffectToOwner(ResolvedShieldGameplayEffectClass, FMath::Max(GetAbilityLevel(), 1), 1);
	if (K2_HasAuthority() && !EffectHandle.WasSuccessfullyApplied())
	{
		CancelAbilityForSkillExecutionFailure();
	}
}

void UShieldAbility::CleanupShieldTasks()
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

void UShieldAbility::HandleShieldMontageFinished()
{
	ShieldMontageTask = nullptr;
	if (!bShieldApplied)
	{
		ApplyShieldFromMontageTrigger();
	}
	if (IsEndAbilityValid(CurrentSpecHandle, CurrentActorInfo))
	{
		K2_EndAbility();
	}
}

void UShieldAbility::HandleMontageTriggerEvent(FGameplayEventData Payload)
{

ApplyShieldFromMontageTrigger();
}
