#include "AbilitySystem/PdAbilitySystemComponent.h"

#include "AbilitySystemGlobals.h"
#include "Common/ProjectTagConfig.h"
#include "GameplayEffect.h"
#include "GameFramework/Actor.h"
#include "Net/Core/PushModel/PushModel.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PdAbilitySystemComponent)

DEFINE_LOG_CATEGORY_STATIC(PdAbilitySystemComponentLog, Log, All);

namespace
{
	bool SetAttributeDataDefaultValue(UAttributeSet* AttributeSet, const FGameplayAttribute& Attribute, float DefaultValue)
	{
		if (!AttributeSet || !Attribute.IsValid())
		{
			return false;
		}

		if (FGameplayAttributeData* AttributeData = Attribute.GetGameplayAttributeData(AttributeSet))
		{
			AttributeData->SetBaseValue(DefaultValue);
			AttributeData->SetCurrentValue(DefaultValue);
			return true;
		}

		float NewValue = DefaultValue;
		Attribute.SetNumericValueChecked(NewValue, AttributeSet);
		return true;
	}

}

UPdAbilitySystemComponent::UPdAbilitySystemComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
	SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
}

void UPdAbilitySystemComponent::OnRegister()
{
	if (!AbilityActorInfo.IsValid())
	{
		AbilityActorInfo = TSharedPtr<FGameplayAbilityActorInfo>(UAbilitySystemGlobals::Get().AllocAbilityActorInfo());
	}

	Super::OnRegister();
}

int32 UPdAbilitySystemComponent::AddAttributeConfig(const FPdAttributeConfig& AttributeConfig)
{
	if (!AttributeConfig.HasAnyData())
	{
		return INDEX_NONE;
	}

	const int32 AttributeConfigHandle = NextAttributeConfigHandle++;
	ActiveAttributeConfigs.Add(AttributeConfigHandle, AttributeConfig);
	AttributeConfigOrder.Add(AttributeConfigHandle);
	return AttributeConfigHandle;
}

void UPdAbilitySystemComponent::RemoveAttributeConfig(int32 AttributeConfigHandle)
{
	if (AttributeConfigHandle == INDEX_NONE)
	{
		return;
	}

	ActiveAttributeConfigs.Remove(AttributeConfigHandle);
	AttributeConfigOrder.Remove(AttributeConfigHandle);
}

bool UPdAbilitySystemComponent::ApplyAttributeDefaultValues(const FPdAttributeConfig& AttributeConfig)
{
	bool bAppliedAny = false;
	TArray<FPdAttributeTagMapping> OrderedMappings = AttributeConfig.AttributeMappings;
	OrderedMappings.StableSort([](const FPdAttributeTagMapping& Left, const FPdAttributeTagMapping& Right)
	{
		return Left.DefaultValuePriority < Right.DefaultValuePriority;
	});

	for (const FPdAttributeTagMapping& Entry : OrderedMappings)
	{
		if (!Entry.IsValid())
		{
			continue;
		}

		TSubclassOf<UAttributeSet> AttributeSetClass = const_cast<UClass*>(Entry.Attribute.GetAttributeSetClass());
		if (!AttributeSetClass)
		{
			UE_LOG(PdAbilitySystemComponentLog, Warning, TEXT("ApplyAttributeDefaultValues skipped '%s': attribute has no AttributeSet class."),
				*Entry.StatTag.ToString());
			continue;
		}

		UAttributeSet* AttributeSet = const_cast<UAttributeSet*>(GetAttributeSet(AttributeSetClass));
		FProperty* Property = Entry.Attribute.GetUProperty();
		if (!AttributeSet || !Property)
		{
			UE_LOG(PdAbilitySystemComponentLog, Warning,
				TEXT("ApplyAttributeDefaultValues skipped '%s' -> '%s': missing AttributeSet or property on '%s'."),
				*Entry.StatTag.ToString(),
				*Entry.Attribute.GetName(),
				*GetNameSafe(GetOwner()));
			continue;
		}

		SetNumericAttributeBase(Entry.Attribute, Entry.DefaultValue);
		if (!SetAttributeDataDefaultValue(AttributeSet, Entry.Attribute, Entry.DefaultValue))
		{
			UE_LOG(PdAbilitySystemComponentLog, Warning,
				TEXT("ApplyAttributeDefaultValues failed '%s' -> '%s' on '%s'."),
				*Entry.StatTag.ToString(),
				*Entry.Attribute.GetName(),
				*GetNameSafe(GetOwner()));
			continue;
		}

		MARK_PROPERTY_DIRTY(AttributeSet, Property);
		bAppliedAny = true;
	}

	if (bAppliedAny)
	{
		if (AActor* OwningActor = GetOwner())
		{
			OwningActor->ForceNetUpdate();
		}
	}

	return bAppliedAny;
}

void UPdAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	ABILITYLIST_SCOPE_LOCK();
	for (FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
	{
		if (!AbilitySpec.Ability || !AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			continue;
		}

		AbilitySpec.InputPressed = true;
		if (AbilitySpec.IsActive())
		{
			AbilitySpecInputPressed(AbilitySpec);
			continue;
		}

		TryActivateAbility(AbilitySpec.Handle);
	}
}

void UPdAbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	ABILITYLIST_SCOPE_LOCK();
	for (FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
	{
		if (!AbilitySpec.Ability || !AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			continue;
		}

		AbilitySpec.InputPressed = false;
		if (AbilitySpec.IsActive())
		{
			AbilitySpecInputReleased(AbilitySpec);
		}
	}
}

bool UPdAbilitySystemComponent::ApplyStatUpEffectByTag(TSubclassOf<UGameplayEffect> GameplayEffectClass, FGameplayTag StatTag, float Magnitude,
	EEnum_Operation Operation, float Level)
{
	UE_LOG(PdAbilitySystemComponentLog, Log, TEXT("[StatUpgrade] ASC ApplyStatUpEffectByTag: asc=%s owner=%s effect=%s tag=%s magnitude=%.3f operation=%d level=%.3f"),
		*GetNameSafe(this),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(GameplayEffectClass.Get()),
		*StatTag.ToString(),
		Magnitude,
		static_cast<int32>(Operation),
		Level);

	if (!StatTag.IsValid() || FMath::IsNearlyZero(Magnitude))
	{
		UE_LOG(PdAbilitySystemComponentLog, Warning, TEXT("[StatUpgrade] ASC ApplyStatUpEffectByTag rejected: tag=%s valid=%s magnitude=%.3f"),
			*StatTag.ToString(),
			StatTag.IsValid() ? TEXT("true") : TEXT("false"),
			Magnitude);
		return false;
	}

	TMap<FGameplayTag, float> StatMagnitudes;
	StatMagnitudes.Add(StatTag, Magnitude);
	return ApplyStatUpEffectByTags(GameplayEffectClass, StatMagnitudes, Operation, Level);
}

bool UPdAbilitySystemComponent::ApplyStatUpEffectByTags(TSubclassOf<UGameplayEffect> GameplayEffectClass,
	const TMap<FGameplayTag, float>& StatMagnitudes, EEnum_Operation Operation, float Level)
{
	FGameplayTag OperationSetByCallerTag;
	if (!GameplayEffectClass || StatMagnitudes.IsEmpty() || !ResolveStatUpOperationSetByCallerTag(OperationSetByCallerTag))
	{
		UE_LOG(PdAbilitySystemComponentLog, Warning, TEXT("[StatUpgrade] ASC ApplyStatUpEffectByTags rejected: effect=%s statCount=%d operationTag=%s"),
			*GetNameSafe(GameplayEffectClass.Get()),
			StatMagnitudes.Num(),
			*OperationSetByCallerTag.ToString());
		return false;
	}
	UE_LOG(PdAbilitySystemComponentLog, Log, TEXT("[StatUpgrade] ASC ApplyStatUpEffectByTags started: effect=%s statCount=%d operationTag=%s operation=%d level=%.3f"),
		*GetNameSafe(GameplayEffectClass.Get()),
		StatMagnitudes.Num(),
		*OperationSetByCallerTag.ToString(),
		static_cast<int32>(Operation),
		Level);

	FGameplayEffectContextHandle EffectContext = MakeEffectContext();
	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingSpec(GameplayEffectClass, Level, EffectContext);
	if (!SpecHandle.IsValid())
	{
		UE_LOG(PdAbilitySystemComponentLog, Warning, TEXT("[StatUpgrade] ASC ApplyStatUpEffectByTags failed: invalid spec. effect=%s"),
			*GetNameSafe(GameplayEffectClass.Get()));
		return false;
	}

	bool bAddedAnyMagnitude = false;
	for (const TPair<FGameplayTag, float>& Pair : StatMagnitudes)
	{
		if (!Pair.Key.IsValid() || FMath::IsNearlyZero(Pair.Value))
		{
			continue;
		}

		SpecHandle.Data->SetSetByCallerMagnitude(Pair.Key, Pair.Value);
		bAddedAnyMagnitude = true;
		UE_LOG(PdAbilitySystemComponentLog, Log, TEXT("[StatUpgrade] ASC SetByCaller stat magnitude: tag=%s value=%.3f"),
			*Pair.Key.ToString(),
			Pair.Value);
	}

	if (!bAddedAnyMagnitude)
	{
		UE_LOG(PdAbilitySystemComponentLog, Warning, TEXT("[StatUpgrade] ASC ApplyStatUpEffectByTags failed: no valid stat magnitudes."));
		return false;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(OperationSetByCallerTag, static_cast<float>(Operation));
	UE_LOG(PdAbilitySystemComponentLog, Log, TEXT("[StatUpgrade] ASC SetByCaller operation: tag=%s value=%d"),
		*OperationSetByCallerTag.ToString(),
		static_cast<int32>(Operation));

	const FActiveGameplayEffectHandle AppliedHandle = ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	const bool bApplied = AppliedHandle.WasSuccessfullyApplied();
	UE_LOG(PdAbilitySystemComponentLog, Log, TEXT("[StatUpgrade] ASC ApplyGameplayEffectSpecToSelf result: effect=%s handle=%s result=%s"),
		*GetNameSafe(GameplayEffectClass.Get()),
		*AppliedHandle.ToString(),
		bApplied ? TEXT("true") : TEXT("false"));
	return bApplied;
}

bool UPdAbilitySystemComponent::ResolveAttributeFromTag(const FGameplayTag& StatTag, FGameplayAttribute& OutAttribute) const
{
	OutAttribute = FGameplayAttribute();

	if (!StatTag.IsValid())
	{
		return false;
	}

	for (int32 Index = AttributeConfigOrder.Num() - 1; Index >= 0; --Index)
	{
		const FPdAttributeConfig* AttributeConfig = ActiveAttributeConfigs.Find(AttributeConfigOrder[Index]);
		if (!AttributeConfig)
		{
			continue;
		}

		for (const FPdAttributeTagMapping& Entry : AttributeConfig->AttributeMappings)
		{
			if (Entry.StatTag.MatchesTagExact(StatTag) && Entry.Attribute.IsValid())
			{
				OutAttribute = Entry.Attribute;
				UE_LOG(PdAbilitySystemComponentLog, Log, TEXT("[StatUpgrade] ResolveAttributeFromTag matched: statTag=%s attribute=%s configHandle=%d"),
					*StatTag.ToString(),
					*OutAttribute.GetName(),
					AttributeConfigOrder[Index]);
				return true;
			}
		}
	}

	UE_LOG(PdAbilitySystemComponentLog, Warning, TEXT("[StatUpgrade] ResolveAttributeFromTag failed: statTag=%s activeConfigCount=%d"),
		*StatTag.ToString(),
		ActiveAttributeConfigs.Num());
	return false;
}

bool UPdAbilitySystemComponent::ResolveDamageMagnitudeSetByCallerTag(FGameplayTag& OutTag) const
{
	OutTag = UProjectTagConfig::Get(this)->GetSetByCallerDamageMagnitudeTag();
	return OutTag.IsValid();
}

bool UPdAbilitySystemComponent::ResolveStatUpOperationSetByCallerTag(FGameplayTag& OutTag) const
{
	OutTag = UProjectTagConfig::Get(this)->GetSetByCallerStatUpOperationTag();
	UE_LOG(PdAbilitySystemComponentLog, Log, TEXT("[StatUpgrade] ResolveStatUpOperationSetByCallerTag: tag=%s valid=%s"),
		*OutTag.ToString(),
		OutTag.IsValid() ? TEXT("true") : TEXT("false"));
	return OutTag.IsValid();
}
