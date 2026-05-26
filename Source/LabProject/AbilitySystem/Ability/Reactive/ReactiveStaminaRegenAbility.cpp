#include "AbilitySystem/Ability/Reactive/ReactiveStaminaRegenAbility.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Common/LabGameplayTags.h"
#include "GameplayEffect.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ReactiveStaminaRegenAbility)

DEFINE_LOG_CATEGORY_STATIC(LogReactiveStaminaRegenAbility, Log, All);

UReactiveStaminaRegenAbility::UReactiveStaminaRegenAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAutoActivateWhenGranted = true;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	ActivationOwnedTags.Reset();
}

void UReactiveStaminaRegenAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC || !StaminaRegenEffectClass)
	{
		UE_LOG(LogReactiveStaminaRegenAbility, Warning,
			TEXT("Reactive stamina regen ended: missing ASC or StaminaRegenEffectClass. ability=%s asc=%s effect=%s"),
			*GetNameSafe(this),
			*GetNameSafe(ASC),
			*GetNameSafe(StaminaRegenEffectClass.Get()));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (StaminaChangedDelegateHandle.IsValid())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetStaminaAttribute()).Remove(StaminaChangedDelegateHandle);
		StaminaChangedDelegateHandle.Reset();
	}

	StaminaChangedDelegateHandle = ASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetStaminaAttribute())
		.AddUObject(this, &ThisClass::HandleStaminaChanged);
}

void UReactiveStaminaRegenAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	ClearRegenDelayTimer();
	RemoveStaminaRegenEffects();

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		if (StaminaChangedDelegateHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetStaminaAttribute()).Remove(StaminaChangedDelegateHandle);
		}
	}

	StaminaChangedDelegateHandle.Reset();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UReactiveStaminaRegenAbility::HandleStaminaChanged(const FOnAttributeChangeData& Data)
{
	if (FMath::IsNearlyEqual(Data.NewValue, Data.OldValue))
	{
		return;
	}

	if (Data.NewValue < Data.OldValue)
	{
		RemoveStaminaRegenEffects();

		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				StaminaRegenDelayTimerHandle,
				this,
				&ThisClass::ApplyStaminaRegenEffect,
				StaminaRegenDelay,
				false);
		}
		return;
	}

	if (!bRemoveRegenEffectAtMaxStamina)
	{
		return;
	}

	const float MaxStamina = GetCurrentMaxStamina();
	if (MaxStamina > 0.0f && Data.NewValue >= MaxStamina - KINDA_SMALL_NUMBER)
	{
		ClearRegenDelayTimer();
		RemoveStaminaRegenEffects();
	}
}

void UReactiveStaminaRegenAbility::ApplyStaminaRegenEffect()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC || !StaminaRegenEffectClass)
	{
		return;
	}

	const float MaxStamina = GetCurrentMaxStamina();
	const float CurrentStamina = ASC->GetNumericAttribute(UBasicAttributeSet::GetStaminaAttribute());
	if (MaxStamina > 0.0f && CurrentStamina >= MaxStamina - KINDA_SMALL_NUMBER)
	{
		RemoveStaminaRegenEffects();
		return;
	}

	RemoveStaminaRegenEffects();

	FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(StaminaRegenEffectClass, static_cast<float>(GetAbilityLevel()), EffectContext);
	if (SpecHandle.IsValid() && SpecHandle.Data.IsValid())
	{
		ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}
}

void UReactiveStaminaRegenAbility::RemoveStaminaRegenEffects()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		return;
	}

	FGameplayTagContainer RegenTags;
	RegenTags.AddTag(LabGameplayTags::Status_StaminaRegen);
	ASC->RemoveActiveEffectsWithGrantedTags(RegenTags);
}

void UReactiveStaminaRegenAbility::ClearRegenDelayTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StaminaRegenDelayTimerHandle);
	}
}

float UReactiveStaminaRegenAbility::GetCurrentMaxStamina() const
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	return ASC ? ASC->GetNumericAttribute(UBasicAttributeSet::GetMaxStaminaAttribute()) : 0.0f;
}
