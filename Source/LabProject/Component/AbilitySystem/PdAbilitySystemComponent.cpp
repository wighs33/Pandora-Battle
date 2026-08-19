#include "Component/AbilitySystem/PdAbilitySystemComponent.h"

#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "AbilitySystemGlobals.h"
#include "Common/LabGameplayTags.h"
#include "Component/AbilitySystem/AbilityAttributeRuntime.h"
#include "Component/AbilitySystem/AbilityCollectionRuntime.h"
#include "Component/AbilitySystem/AbilityResetRuntime.h"
#include "Engine/World.h"
#include "Pandora/PandoraSkillRuntimeContext.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PdAbilitySystemComponent)

UPdAbilitySystemComponent::UPdAbilitySystemComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
	SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeRuntime =
		ObjectInitializer.CreateDefaultSubobject<UAbilityAttributeRuntime>(
			this,
			TEXT("AttributeRuntime"));
	CollectionRuntime =
		ObjectInitializer.CreateDefaultSubobject<UAbilityCollectionRuntime>(
			this,
			TEXT("CollectionRuntime"));
	ResetRuntime =
		ObjectInitializer.CreateDefaultSubobject<UAbilityResetRuntime>(
			this,
			TEXT("ResetRuntime"));
}

void UPdAbilitySystemComponent::OnRegister()
{
	// Extension condition checks can run before full GAS initialization.
	if (!AbilityActorInfo.IsValid())
	{
		AbilityActorInfo =
			TSharedPtr<FGameplayAbilityActorInfo>(
				UAbilitySystemGlobals::Get().AllocAbilityActorInfo());
	}

	Super::OnRegister();
}

void UPdAbilitySystemComponent::OnGiveAbility(FGameplayAbilitySpec& AbilitySpec)
{
	if (CollectionRuntime)
	{
		CollectionRuntime->CachePandoraSkillRuntimeContext(AbilitySpec.SourceObject.Get());
	}

	Super::OnGiveAbility(AbilitySpec);
	NotifyAbilitiesChanged();
}

void UPdAbilitySystemComponent::OnRemoveAbility(FGameplayAbilitySpec& AbilitySpec)
{
	UPandoraSkillRuntimeContext* RemovedRuntimeContext =
		Cast<UPandoraSkillRuntimeContext>(AbilitySpec.SourceObject.Get());
	const FGameplayAbilitySpecHandle RemovedHandle = AbilitySpec.Handle;

	for (UGameplayAbility* AbilityInstance : AbilitySpec.GetAbilityInstances())
	{
		if (!IsValid(AbilityInstance))
		{
			continue;
		}

		if (UPdGameplayAbility* PdAbilityInstance =
			Cast<UPdGameplayAbility>(AbilityInstance))
		{
			PdAbilityInstance->CleanupConfiguredPresentation();
		}
	}

	Super::OnRemoveAbility(AbilitySpec);

	if (CollectionRuntime)
	{
		CollectionRuntime->ReleasePandoraSkillRuntimeContextIfUnused(
			*this,
			RemovedRuntimeContext,
			RemovedHandle);
	}

	NotifyAbilitiesChanged();
}

void UPdAbilitySystemComponent::NotifyAbilityActivated(
	const FGameplayAbilitySpecHandle Handle,
	UGameplayAbility* Ability)
{
	Super::NotifyAbilityActivated(Handle, Ability);

	const FGameplayAbilitySpec* AbilitySpec =
		FindAbilitySpecFromHandle(Handle);
	const UPdGameplayAbility* PdAbility = Cast<UPdGameplayAbility>(Ability);
	if (!AbilityActorInfo.IsValid()
		|| !AbilityActorInfo->IsLocallyControlled()
		|| !AbilitySpec
		|| AbilitySpec->InputPressed
		|| (PdAbility && !PdAbility->ShouldAutoConfirmOnInputRelease())
		|| !AbilitySpec->GetDynamicSpecSourceTags().HasTagExact(
			LabGameplayTags::Skill_Type_Press))
	{
		return;
	}

	// NotifyAbilityActivated runs from PreActivate, before derived abilities
	// create their targeting tasks. Defer one tick so a release that beat the
	// ServerInitiated activation response can confirm the newly-bound task.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(
				this,
				&ThisClass::ReplayReleasedPressInputAfterActivation,
				Handle));
	}
}

void UPdAbilitySystemComponent::ReplayReleasedPressInputAfterActivation(
	const FGameplayAbilitySpecHandle AbilityHandle)
{
	if (CollectionRuntime)
	{
		CollectionRuntime->ReplayReleasedPressInputAfterActivation(
			*this,
			AbilityHandle);
	}
}

void UPdAbilitySystemComponent::OnRep_ActivateAbilities()
{
	Super::OnRep_ActivateAbilities();

	if (!CollectionRuntime
		|| !CollectionRuntime->HasReplicatedAbilityListChanged(*this))
	{
		return;
	}

	CollectionRuntime->CacheReplicatedAbilityList(*this);
	NotifyAbilitiesChanged();
}

int32 UPdAbilitySystemComponent::AddAttributeConfig(const FAttributeConfig& AttributeConfig)
{
	return AttributeRuntime
		? AttributeRuntime->AddAttributeConfig(AttributeConfig)
		: INDEX_NONE;
}

void UPdAbilitySystemComponent::RemoveAttributeConfig(const int32 AttributeConfigHandle)
{
	if (AttributeRuntime)
	{
		AttributeRuntime->RemoveAttributeConfig(AttributeConfigHandle);
	}
}

bool UPdAbilitySystemComponent::ApplyAttributeDefaultValue(
	const FGameplayAttribute& Attribute,
	const float DefaultValue)
{
	return AttributeRuntime
		&& AttributeRuntime->ApplyAttributeDefaultValue(
			*this,
			Attribute,
			DefaultValue);
}

bool UPdAbilitySystemComponent::HasAppliedConfiguredAttributeDefaults(
	const UAttributeSet* AttributeSet,
	const FSoftObjectPath& DefinitionPath) const
{
	return AttributeSet
		&& !DefinitionPath.IsNull()
		&& ConfiguredDefaultsAttributeSet.Get() == AttributeSet
		&& ConfiguredDefaultsDefinitionPath == DefinitionPath;
}

void UPdAbilitySystemComponent::MarkConfiguredAttributeDefaultsApplied(
	UAttributeSet* AttributeSet,
	const FSoftObjectPath& DefinitionPath)
{
	ConfiguredDefaultsAttributeSet = AttributeSet;
	ConfiguredDefaultsDefinitionPath = DefinitionPath;
}

void UPdAbilitySystemComponent::ClearConfiguredAttributeDefaultsApplied(
	const UAttributeSet* AttributeSet,
	const FSoftObjectPath& DefinitionPath)
{
	if (!HasAppliedConfiguredAttributeDefaults(AttributeSet, DefinitionPath))
	{
		return;
	}

	ConfiguredDefaultsAttributeSet.Reset();
	ConfiguredDefaultsDefinitionPath.Reset();
}

void UPdAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& InputTag)
{
	if (CollectionRuntime)
	{
		CollectionRuntime->AbilityInputTagPressed(*this, InputTag);
	}
}

void UPdAbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag& InputTag)
{
	if (CollectionRuntime)
	{
		CollectionRuntime->AbilityInputTagReleased(*this, InputTag);
	}
}

const FGameplayAbilitySpec* UPdAbilitySystemComponent::FindActiveAbilitySpecByTags(
	const FGameplayTagContainer& AbilityTags) const
{
	return CollectionRuntime
		? CollectionRuntime->FindActiveAbilitySpecByTags(*this, AbilityTags)
		: nullptr;
}

bool UPdAbilitySystemComponent::HasActiveAbilityWithTags(
	const FGameplayTagContainer& AbilityTags) const
{
	return FindActiveAbilitySpecByTags(AbilityTags) != nullptr;
}

bool UPdAbilitySystemComponent::HasActiveAbilityOfClass(
	const TSubclassOf<UGameplayAbility> AbilityClass,
	const bool bIncludeChildClasses) const
{
	return CollectionRuntime
		&& CollectionRuntime->HasActiveAbilityOfClass(
			*this,
			AbilityClass,
			bIncludeChildClasses);
}

bool UPdAbilitySystemComponent::HasActiveAbilityOfAnyClass(
	const TArray<TSubclassOf<UGameplayAbility>>& AbilityClasses,
	const bool bIncludeChildClasses) const
{
	return CollectionRuntime
		&& CollectionRuntime->HasActiveAbilityOfAnyClass(
			*this,
			AbilityClasses,
			bIncludeChildClasses);
}

bool UPdAbilitySystemComponent::ApplyStatUpEffectByTag(
	TSubclassOf<UGameplayEffect> GameplayEffectClass,
	const FGameplayTag StatTag,
	const float Magnitude,
	const EEnum_Operation Operation,
	const float Level)
{
	return AttributeRuntime
		&& AttributeRuntime->ApplyStatUpEffectByTag(
			*this,
			GameplayEffectClass,
			StatTag,
			Magnitude,
			Operation,
			Level);
}

bool UPdAbilitySystemComponent::ApplyStatUpEffectByTags(
	TSubclassOf<UGameplayEffect> GameplayEffectClass,
	const TMap<FGameplayTag, float>& StatMagnitudes,
	const EEnum_Operation Operation,
	const float Level)
{
	return AttributeRuntime
		&& AttributeRuntime->ApplyStatUpEffectByTags(
			*this,
			GameplayEffectClass,
			StatMagnitudes,
			Operation,
			Level);
}

TArray<FGameplayAbilitySpecHandle> UPdAbilitySystemComponent::GrantAbilities(
	const TArray<TSubclassOf<UGameplayAbility>>& AbilityClasses,
	const int32 AbilityLevel,
	UObject* SourceObject)
{
	return CollectionRuntime
		? CollectionRuntime->GrantAbilities(
			*this,
			AbilityClasses,
			AbilityLevel,
			SourceObject)
		: TArray<FGameplayAbilitySpecHandle>();
}

void UPdAbilitySystemComponent::RemoveAbilities(
	const TArray<FGameplayAbilitySpecHandle>& AbilityHandles)
{
	if (CollectionRuntime)
	{
		CollectionRuntime->RemoveAbilities(*this, AbilityHandles);
	}
}

void UPdAbilitySystemComponent::ResetAbilityRuntimeStateForDeath()
{
	if (ResetRuntime && CollectionRuntime)
	{
		ResetRuntime->ResetAbilityRuntimeState(*this, *CollectionRuntime, false);
	}
}

void UPdAbilitySystemComponent::ResetPandoraAbilityRuntimeState()
{
	if (ResetRuntime && CollectionRuntime)
	{
		ResetRuntime->ResetAbilityRuntimeState(*this, *CollectionRuntime, true);
	}
}

int32 UPdAbilitySystemComponent::ClearStatusEffectsForRespawn()
{
	return ResetRuntime
		? ResetRuntime->ClearStatusEffectsForRespawn(*this)
		: 0;
}

void UPdAbilitySystemComponent::ReactivateAutoActivatedAbilities()
{
	if (CollectionRuntime)
	{
		CollectionRuntime->ReactivateAutoActivatedAbilities(*this);
	}
}

bool UPdAbilitySystemComponent::IsResettingAbilityRuntimeState() const
{
	return ResetRuntime && ResetRuntime->IsResettingAbilityRuntimeState();
}

bool UPdAbilitySystemComponent::ResolveAttributeFromTag(
	const FGameplayTag& StatTag,
	FGameplayAttribute& OutAttribute) const
{
	if (AttributeRuntime
		&& AttributeRuntime->ResolveAttributeFromTag(StatTag, OutAttribute))
	{
		return true;
	}

	return UBasicAttributeSet::ResolveAttributeFromStatTag(
		StatTag,
		OutAttribute);
}

bool UPdAbilitySystemComponent::ResolveDamageMagnitudeSetByCallerTag(
	FGameplayTag& OutTag) const
{
	if (!AttributeRuntime)
	{
		OutTag = FGameplayTag();
		return false;
	}

	return AttributeRuntime->ResolveDamageMagnitudeSetByCallerTag(*this, OutTag);
}

bool UPdAbilitySystemComponent::ResolveStatUpOperationSetByCallerTag(
	FGameplayTag& OutTag) const
{
	if (!AttributeRuntime)
	{
		OutTag = FGameplayTag();
		return false;
	}

	return AttributeRuntime->ResolveStatUpOperationSetByCallerTag(*this, OutTag);
}

void UPdAbilitySystemComponent::NotifyAbilitiesChanged()
{
	OnAbilitiesChanged.Broadcast();
	OnAbilitiesChangedNative.Broadcast();

	FGameplayEventData EventData;
	EventData.EventTag = LabGameplayTags::Event_Abilities_Changed;
	EventData.Instigator = GetAvatarActor();
	EventData.Target = GetAvatarActor();
	HandleGameplayEvent(LabGameplayTags::Event_Abilities_Changed, &EventData);
}
