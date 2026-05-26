#include "AbilitySystem/Ability/Reactive/ReactiveStatusEffectAbility.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/Data/PdStatusEffectDataAsset.h"
#include "AbilitySystem/PdAbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEffectApplied_Target.h"
#include "Character/PdCharacterBase.h"
#include "Common/LabGameplayTags.h"
#include "GameplayEffect.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ReactiveStatusEffectAbility)

DEFINE_LOG_CATEGORY_STATIC(LogReactiveStatusEffectAbility, Log, All);

UReactiveStatusEffectAbility::UReactiveStatusEffectAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAutoActivateWhenGranted = true;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	ActivationOwnedTags.Reset();
}

void UReactiveStatusEffectAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!StatusEffectDataAsset)
	{
		UE_LOG(LogReactiveStatusEffectAbility, Warning,
			TEXT("Reactive status ability ended: StatusEffectDataAsset is null. ability=%s"),
			*GetNameSafe(this));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UE_LOG(LogReactiveStatusEffectAbility, Log,
		TEXT("Reactive status ability activated: ability=%s owner=%s statusData=%s debuff=%s maxStack=%d statusEffect=%s"),
		*GetNameSafe(this),
		*GetNameSafe(GetAvatarActorFromActorInfo()),
		*GetNameSafe(StatusEffectDataAsset.Get()),
		*StatusEffectDataAsset->DebuffTag.ToString(),
		FMath::Max(StatusEffectDataAsset->MaxStackCount, 1),
		*GetNameSafe(StatusEffectDataAsset->StatusEffectClass.Get()));

	WaitGameplayEffectAppliedTask = UAbilityTask_WaitGameplayEffectApplied_Target::WaitGameplayEffectAppliedToTarget(
		this,
		FGameplayTargetDataFilterHandle(),
		FGameplayTagRequirements(),
		FGameplayTagRequirements(),
		FGameplayTagRequirements(),
		FGameplayTagRequirements(),
		false,
		nullptr,
		false);

	if (!WaitGameplayEffectAppliedTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	WaitGameplayEffectAppliedTask->OnApplied.AddDynamic(this, &ThisClass::OnGameplayEffectAppliedToTarget);
	WaitGameplayEffectAppliedTask->ReadyForActivation();
}

void UReactiveStatusEffectAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (WaitGameplayEffectAppliedTask)
	{
		WaitGameplayEffectAppliedTask->EndTask();
		WaitGameplayEffectAppliedTask = nullptr;
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

FGameplayEffectSpecHandle UReactiveStatusEffectAbility::ModifyEffectSpecBeforeApplication_Implementation(FGameplayEffectSpecHandle SpecHandle)
{
	return SpecHandle;
}

void UReactiveStatusEffectAbility::NotifyStackCountChanged_Implementation(AActor* TargetActor, int32 NewStackCount)
{
	if (!IsValid(TargetActor) || !StatusEffectDataAsset)
	{
		return;
	}

	APdCharacterBase* AvatarCharacter = Cast<APdCharacterBase>(GetAvatarActorFromActorInfo());
	if (!AvatarCharacter)
	{
		return;
	}

	FGameplayEventData EventData;
	EventData.EventTag = LabGameplayTags::Event_Effect_StackCountChanged;
	EventData.Instigator = AvatarCharacter;
	EventData.Target = TargetActor;
	EventData.TargetTags.AddTag(StatusEffectDataAsset->DebuffTag);
	EventData.EventMagnitude = static_cast<float>(NewStackCount);

	AvatarCharacter->MulticastSendGameplayEventToActor(TargetActor, EventData);
}

void UReactiveStatusEffectAbility::OnGameplayEffectAppliedToTarget(
	AActor* TargetActor,
	FGameplayEffectSpecHandle SpecHandle,
	FActiveGameplayEffectHandle ActiveHandle)
{
	if (!IsValid(TargetActor) || !StatusEffectDataAsset)
	{
		return;
	}

	const UGameplayEffect* AppliedGameplayEffect = SpecHandle.Data.IsValid() ? SpecHandle.Data->Def.Get() : nullptr;
	if (!AppliedGameplayEffect || !AppliedGameplayEffect->GetGrantedTags().HasTag(StatusEffectDataAsset->DebuffTag))
	{
		return;
	}

	const int32 StackCount = GetDebuffStackCount(TargetActor, ActiveHandle);
	const int32 RequiredStackCount = FMath::Max(StatusEffectDataAsset->MaxStackCount, 1);
	UE_LOG(LogReactiveStatusEffectAbility, Log,
		TEXT("Reactive status debuff detected: ability=%s target=%s debuff=%s stack=%d required=%d effect=%s"),
		*GetNameSafe(this),
		*GetNameSafe(TargetActor),
		*StatusEffectDataAsset->DebuffTag.ToString(),
		StackCount,
		RequiredStackCount,
		*GetNameSafe(AppliedGameplayEffect));
	if (StackCount < RequiredStackCount)
	{
		NotifyStackCountChanged(TargetActor, StackCount);
		return;
	}

	if (!StatusEffectDataAsset->StatusEffectClass)
	{
		UE_LOG(LogReactiveStatusEffectAbility, Warning,
			TEXT("Reactive status skipped: StatusEffectClass is null. ability=%s target=%s statusData=%s"),
			*GetNameSafe(this),
			*GetNameSafe(TargetActor),
			*GetNameSafe(StatusEffectDataAsset.Get()));
		return;
	}

	FGameplayEffectSpecHandle StatusEffectSpec = MakeOutgoingGameplayEffectSpec(
		StatusEffectDataAsset->StatusEffectClass,
		static_cast<float>(GetAbilityLevel()));

	if (!StatusEffectSpec.IsValid())
	{
		return;
	}

	if (StatusEffectDataAsset->StatusDuration > 0.0f)
	{
		StatusEffectSpec = UAbilitySystemBlueprintLibrary::SetDuration(StatusEffectSpec, StatusEffectDataAsset->StatusDuration);
	}

	ApplyDefaultSetByCallerMagnitudes(StatusEffectSpec);
	StatusEffectSpec = ModifyEffectSpecBeforeApplication(StatusEffectSpec);
	const FGameplayAbilityTargetDataHandle TargetData = UAbilitySystemBlueprintLibrary::AbilityTargetDataFromActor(TargetActor);
	UE_LOG(LogReactiveStatusEffectAbility, Log,
		TEXT("Reactive status applying: ability=%s target=%s statusEffect=%s level=%d"),
		*GetNameSafe(this),
		*GetNameSafe(TargetActor),
		*GetNameSafe(StatusEffectDataAsset->StatusEffectClass.Get()),
		GetAbilityLevel());
	K2_ApplyGameplayEffectSpecToTarget(StatusEffectSpec, TargetData);
}

void UReactiveStatusEffectAbility::ApplyDefaultSetByCallerMagnitudes(FGameplayEffectSpecHandle& SpecHandle) const
{
	if (!SpecHandle.Data.IsValid() || !StatusEffectDataAsset || StatusEffectDataAsset->StatusDamageMagnitude <= 0.0f)
	{
		return;
	}

	FGameplayTag DamageDataTag = LabGameplayTags::Data_Damage;
	if (const UPdAbilitySystemComponent* PdASC = Cast<UPdAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo()))
	{
		PdASC->ResolveDamageMagnitudeSetByCallerTag(DamageDataTag);
	}

	if (DamageDataTag.IsValid())
	{
		SpecHandle.Data->SetSetByCallerMagnitude(DamageDataTag, StatusEffectDataAsset->StatusDamageMagnitude);
	}
}

int32 UReactiveStatusEffectAbility::GetDebuffStackCount(AActor* TargetActor, FActiveGameplayEffectHandle ActiveHandle) const
{
	if (!IsValid(TargetActor) || !StatusEffectDataAsset || !StatusEffectDataAsset->DebuffTag.IsValid())
	{
		return ActiveHandle.IsValid() ? 1 : 0;
	}

	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
	if (!TargetASC)
	{
		return ActiveHandle.IsValid() ? 1 : 0;
	}

	FGameplayTagContainer DebuffTags;
	DebuffTags.AddTag(StatusEffectDataAsset->DebuffTag);

	int32 StackCount = 0;
	const TArray<FActiveGameplayEffectHandle> ActiveDebuffHandles = TargetASC->GetActiveEffectsWithAllTags(DebuffTags);
	for (const FActiveGameplayEffectHandle& DebuffHandle : ActiveDebuffHandles)
	{
		StackCount += FMath::Max(TargetASC->GetCurrentStackCount(DebuffHandle), 1);
	}

	if (StackCount <= 0 && ActiveHandle.IsValid())
	{
		StackCount = FMath::Max(TargetASC->GetCurrentStackCount(ActiveHandle), 1);
	}

	if (StackCount <= 0 && TargetASC->HasMatchingGameplayTag(StatusEffectDataAsset->DebuffTag))
	{
		StackCount = 1;
	}

	return StackCount;
}
