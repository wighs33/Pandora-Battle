#include "AbilitySystem/Ability/Reactive/ReactiveStatusEffectAbility.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Definition/AbilitySystem/StatusEffectDefinition.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEffectApplied_Target.h"
#include "Character/CharacterBase.h"
#include "Common/LabGameplayTags.h"
#include "GameplayEffect.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ReactiveStatusEffectAbility)

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

		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}



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

	ACharacterBase* AvatarCharacter = Cast<ACharacterBase>(GetAvatarActorFromActorInfo());
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

	if (StackCount < RequiredStackCount)
	{
		NotifyStackCountChanged(TargetActor, StackCount);
		return;
	}

	if (!StatusEffectDataAsset->StatusEffectClass)
	{

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
	StatusEffectDataAsset->AppendRemovalPolicyTags(StatusEffectSpec);
	if (StatusEffectDataAsset->StatusEffectTag.IsValid() && StatusEffectSpec.Data.IsValid())
	{
		StatusEffectSpec.Data->DynamicGrantedTags.AddTag(StatusEffectDataAsset->StatusEffectTag);
	}

	const FGameplayAbilityTargetDataHandle TargetData = UAbilitySystemBlueprintLibrary::AbilityTargetDataFromActor(TargetActor);

	K2_ApplyGameplayEffectSpecToTarget(StatusEffectSpec, TargetData);
}

void UReactiveStatusEffectAbility::ApplyDefaultSetByCallerMagnitudes(FGameplayEffectSpecHandle& SpecHandle) const
{
	if (!SpecHandle.Data.IsValid() || !StatusEffectDataAsset || StatusEffectDataAsset->ResolveDamageMagnitude() <= 0.0)
	{
		return;
	}

	FSkillGameplayEffectConfig DamageConfig;
	DamageConfig.Magnitude = StatusEffectDataAsset->ResolveDamageMagnitude();
	const float SkillScaledDamage = CalculateSkillDamageMagnitude(DamageConfig);
	StatusEffectDataAsset->SetDamageMagnitude(
		SpecHandle,
		GetAbilitySystemComponentFromActorInfo(),
		SkillScaledDamage);
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
