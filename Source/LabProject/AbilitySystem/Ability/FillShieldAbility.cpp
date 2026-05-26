#include "AbilitySystem/Ability/FillShieldAbility.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimMontage.h"
#include "Common/LabGameplayTags.h"
#include "GameplayEffect.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(FillShieldAbility)

DEFINE_LOG_CATEGORY_STATIC(LogPandoraFillShieldAbility, Log, All);

UFillShieldAbility::UFillShieldAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	MontageTriggerEventTag = LabGameplayTags::Event_Montage_Trigger;

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
	StartWaitMontageTriggerTask();

	UE_LOG(LogPandoraFillShieldAbility, Log,
		TEXT("Activate: ability=%s avatar=%s authority=%s montage=%s effect=%s trigger=%s"),
		*GetNameSafe(this),
		*GetNameSafe(ActorInfo->AvatarActor.Get()),
		ActorInfo->AvatarActor->HasAuthority() ? TEXT("true") : TEXT("false"),
		*GetNameSafe(FillShieldMontage.Get()),
		*GetNameSafe(FillShieldGameplayEffectClass.Get()),
		*MontageTriggerEventTag.ToString());

	if (!FillShieldMontage)
	{
		UE_LOG(LogPandoraFillShieldAbility, Warning,
			TEXT("Activate without montage: applying fill shield immediately. ability=%s avatar=%s"),
			*GetNameSafe(this),
			*GetNameSafe(ActorInfo->AvatarActor.Get()));
		ApplyFillShieldFromMontageTrigger();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
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

void UFillShieldAbility::StartWaitMontageTriggerTask()
{
	if (!MontageTriggerEventTag.IsValid())
	{
		UE_LOG(LogPandoraFillShieldAbility, Warning,
			TEXT("Wait trigger skipped: invalid montage trigger tag. ability=%s"),
			*GetNameSafe(this));
		return;
	}

	WaitMontageTriggerTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		MontageTriggerEventTag,
		nullptr,
		false,
		true);
	if (!WaitMontageTriggerTask)
	{
		UE_LOG(LogPandoraFillShieldAbility, Warning,
			TEXT("Wait trigger task creation failed. ability=%s tag=%s"),
			*GetNameSafe(this),
			*MontageTriggerEventTag.ToString());
		return;
	}

	WaitMontageTriggerTask->EventReceived.AddDynamic(this, &ThisClass::HandleMontageTriggerEvent);
	WaitMontageTriggerTask->ReadyForActivation();
}

bool UFillShieldAbility::StartFillShieldMontageTask()
{
	FillShieldMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		FillShieldMontage,
		1.0f,
		NAME_None,
		true,
		1.0f,
		0.0f,
		true);
	if (!FillShieldMontageTask)
	{
		UE_LOG(LogPandoraFillShieldAbility, Warning,
			TEXT("Montage task creation failed. ability=%s montage=%s"),
			*GetNameSafe(this),
			*GetNameSafe(FillShieldMontage.Get()));
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
	if (bFillShieldApplied)
	{
		return;
	}
	bFillShieldApplied = true;

	if (!CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
	{
		UE_LOG(LogPandoraFillShieldAbility, Warning,
			TEXT("Fill shield commit failed: ability=%s avatar=%s"),
			*GetNameSafe(this),
			*GetNameSafe(GetAvatarActorFromActorInfo()));
		K2_EndAbility();
		return;
	}

	if (!FillShieldGameplayEffectClass)
	{
		UE_LOG(LogPandoraFillShieldAbility, Warning,
			TEXT("Fill shield effect missing: ability=%s avatar=%s"),
			*GetNameSafe(this),
			*GetNameSafe(GetAvatarActorFromActorInfo()));
		return;
	}

	const FActiveGameplayEffectHandle EffectHandle =
		BP_ApplyGameplayEffectToOwner(FillShieldGameplayEffectClass, FMath::Max(GetAbilityLevel(), 1), 1);
	UE_LOG(LogPandoraFillShieldAbility, Log,
		TEXT("Fill shield applied: ability=%s avatar=%s effect=%s handleValid=%s level=%d"),
		*GetNameSafe(this),
		*GetNameSafe(GetAvatarActorFromActorInfo()),
		*GetNameSafe(FillShieldGameplayEffectClass.Get()),
		EffectHandle.IsValid() ? TEXT("true") : TEXT("false"),
		FMath::Max(GetAbilityLevel(), 1));
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
	K2_EndAbility();
}

void UFillShieldAbility::HandleMontageTriggerEvent(FGameplayEventData Payload)
{
	UE_LOG(LogPandoraFillShieldAbility, Log,
		TEXT("Montage trigger received: ability=%s event=%s avatar=%s"),
		*GetNameSafe(this),
		*Payload.EventTag.ToString(),
		*GetNameSafe(GetAvatarActorFromActorInfo()));

	ApplyFillShieldFromMontageTrigger();
}
