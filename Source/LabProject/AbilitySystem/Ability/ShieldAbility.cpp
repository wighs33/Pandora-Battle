#include "AbilitySystem/Ability/ShieldAbility.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimMontage.h"
#include "Common/LabGameplayTags.h"
#include "GameplayEffect.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ShieldAbility)

DEFINE_LOG_CATEGORY_STATIC(LogPandoraShieldAbility, Log, All);

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
	ActivationBlockedTags.AddTag(LabGameplayTags::Status_Buff_Shield);
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
	StartWaitMontageTriggerTask();

	UE_LOG(LogPandoraShieldAbility, Log,
		TEXT("Activate: ability=%s avatar=%s authority=%s montage=%s effect=%s trigger=%s"),
		*GetNameSafe(this),
		*GetNameSafe(ActorInfo->AvatarActor.Get()),
		ActorInfo->AvatarActor->HasAuthority() ? TEXT("true") : TEXT("false"),
		*GetNameSafe(ShieldMontage.Get()),
		*GetNameSafe(ShieldGameplayEffectClass.Get()),
		*MontageTriggerEventTag.ToString());

	if (!ShieldMontage)
	{
		UE_LOG(LogPandoraShieldAbility, Warning,
			TEXT("Activate without montage: applying shield immediately. ability=%s avatar=%s"),
			*GetNameSafe(this),
			*GetNameSafe(ActorInfo->AvatarActor.Get()));
		ApplyShieldFromMontageTrigger();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	if (!StartShieldMontageTask())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
}

void UShieldAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	CleanupShieldTasks();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UShieldAbility::StartWaitMontageTriggerTask()
{
	if (!MontageTriggerEventTag.IsValid())
	{
		UE_LOG(LogPandoraShieldAbility, Warning,
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
		UE_LOG(LogPandoraShieldAbility, Warning,
			TEXT("Wait trigger task creation failed. ability=%s tag=%s"),
			*GetNameSafe(this),
			*MontageTriggerEventTag.ToString());
		return;
	}

	WaitMontageTriggerTask->EventReceived.AddDynamic(this, &ThisClass::HandleMontageTriggerEvent);
	WaitMontageTriggerTask->ReadyForActivation();
}

bool UShieldAbility::StartShieldMontageTask()
{
	ShieldMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		ShieldMontage,
		1.0f,
		NAME_None,
		true,
		1.0f,
		0.0f,
		true);
	if (!ShieldMontageTask)
	{
		UE_LOG(LogPandoraShieldAbility, Warning,
			TEXT("Montage task creation failed. ability=%s montage=%s"),
			*GetNameSafe(this),
			*GetNameSafe(ShieldMontage.Get()));
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
	if (bShieldApplied)
	{
		return;
	}
	bShieldApplied = true;

	if (!CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
	{
		UE_LOG(LogPandoraShieldAbility, Warning,
			TEXT("Shield commit failed: ability=%s avatar=%s"),
			*GetNameSafe(this),
			*GetNameSafe(GetAvatarActorFromActorInfo()));
		K2_EndAbility();
		return;
	}

	if (!ShieldGameplayEffectClass)
	{
		UE_LOG(LogPandoraShieldAbility, Warning,
			TEXT("Shield effect missing: ability=%s avatar=%s"),
			*GetNameSafe(this),
			*GetNameSafe(GetAvatarActorFromActorInfo()));
		return;
	}

	const FActiveGameplayEffectHandle EffectHandle =
		BP_ApplyGameplayEffectToOwner(ShieldGameplayEffectClass, FMath::Max(GetAbilityLevel(), 1), 1);
	UE_LOG(LogPandoraShieldAbility, Log,
		TEXT("Shield applied: ability=%s avatar=%s effect=%s handleValid=%s level=%d"),
		*GetNameSafe(this),
		*GetNameSafe(GetAvatarActorFromActorInfo()),
		*GetNameSafe(ShieldGameplayEffectClass.Get()),
		EffectHandle.IsValid() ? TEXT("true") : TEXT("false"),
		FMath::Max(GetAbilityLevel(), 1));
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
	K2_EndAbility();
}

void UShieldAbility::HandleMontageTriggerEvent(FGameplayEventData Payload)
{
	UE_LOG(LogPandoraShieldAbility, Log,
		TEXT("Montage trigger received: ability=%s event=%s avatar=%s"),
		*GetNameSafe(this),
		*Payload.EventTag.ToString(),
		*GetNameSafe(GetAvatarActorFromActorInfo()));

	ApplyShieldFromMontageTrigger();
}
