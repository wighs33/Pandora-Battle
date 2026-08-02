#include "AbilitySystem/Ability/FillShieldAbility.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimMontage.h"
#include "Common/LabGameplayTags.h"
#include "GameplayEffect.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(FillShieldAbility)

UFillShieldAbility::UFillShieldAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	MontageTriggerEventTag = LabGameplayTags::Event_Montage_Trigger;

	static ConstructorHelpers::FObjectFinder<UAnimMontage> DefaultFillShieldMontage(
		TEXT("/Game/Animation/Stickman/Axe/Montage_AxeCastShield.Montage_AxeCastShield"));
	if (DefaultFillShieldMontage.Succeeded())
	{
		FillShieldMontage = DefaultFillShieldMontage.Object;
	}

	static ConstructorHelpers::FClassFinder<UGameplayEffect> DefaultFillShieldEffect(
		TEXT("/Game/GAS/Effect/GE_FillShield"));
	if (DefaultFillShieldEffect.Succeeded())
	{
		FillShieldGameplayEffectClass = DefaultFillShieldEffect.Class;
	}

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
	const UAnimMontage* ResolvedFillShieldMontage = GetResolvedFillShieldMontage();
	StartWaitMontageTriggerTask();



	if (!ResolvedFillShieldMontage)
	{

		ApplyFillShieldFromMontageTrigger();
		if (IsEndAbilityValid(Handle, ActorInfo))
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		}
		return;
	}

	if (!StartFillShieldMontageTask())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
}

void UFillShieldAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	CleanupFillShieldTasks();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

const FShieldSkillConfig* UFillShieldAbility::GetFillShieldSkillConfig() const
{
	const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	return SkillDataAsset ? SkillDataAsset->GetDefensiveSkillConfig() : nullptr;
}

UAnimMontage* UFillShieldAbility::GetResolvedFillShieldMontage() const
{
	const FShieldSkillConfig* FillShieldConfig = GetFillShieldSkillConfig();
	return FillShieldConfig && FillShieldConfig->Animation.PrimaryMontage
		? FillShieldConfig->Animation.PrimaryMontage.Get()
		: FillShieldMontage.Get();
}

TSubclassOf<UGameplayEffect> UFillShieldAbility::GetResolvedFillShieldGameplayEffectClass() const
{
	const FShieldSkillConfig* FillShieldConfig = GetFillShieldSkillConfig();
	return FillShieldConfig && FillShieldConfig->GameplayEffectClass
		? FillShieldConfig->GameplayEffectClass
		: FillShieldGameplayEffectClass;
}

FGameplayTag UFillShieldAbility::GetResolvedMontageTriggerEventTag() const
{
	const FShieldSkillConfig* FillShieldConfig = GetFillShieldSkillConfig();
	return FillShieldConfig && FillShieldConfig->Animation.PrimaryEventTag.IsValid()
		? FillShieldConfig->Animation.PrimaryEventTag
		: MontageTriggerEventTag;
}

void UFillShieldAbility::StartWaitMontageTriggerTask()
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

bool UFillShieldAbility::StartFillShieldMontageTask()
{
	UAnimMontage* ResolvedFillShieldMontage = GetResolvedFillShieldMontage();
	FillShieldMontageTask = CreateDefaultMontageAndWaitTask(ResolvedFillShieldMontage);
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
	const TSubclassOf<UGameplayEffect> ResolvedFillShieldGameplayEffectClass =
		GetResolvedFillShieldGameplayEffectClass();
	if (!ResolvedFillShieldGameplayEffectClass)
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

	SpawnConfiguredCharacterDecal();
	const FActiveGameplayEffectHandle EffectHandle =
		BP_ApplyGameplayEffectToOwner(ResolvedFillShieldGameplayEffectClass, FMath::Max(GetAbilityLevel(), 1), 1);
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
