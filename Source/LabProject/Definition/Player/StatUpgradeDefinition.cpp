#include "Definition/Player/StatUpgradeDefinition.h"

#include "Common/LabGameplayTags.h"
#include "GameplayEffect.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(StatUpgradeDefinition)

DEFINE_LOG_CATEGORY_STATIC(StatUpgradeDefinitionLog, Log, All);

namespace
{
	constexpr float MaxSupportedInvestedLevel = 100.f;

#if WITH_EDITOR
	void MarkStatUpgradeInvalid(FDataValidationContext& Context, EDataValidationResult& Result, const FText& Message)
	{
		Result = EDataValidationResult::Invalid;
		Context.AddError(Message);
	}

	bool IsFinite(const float Value)
	{
		return FMath::IsFinite(Value);
	}

	void ValidateFinite(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const float Value,
		const FText& FieldName)
	{
		if (!IsFinite(Value))
		{
			MarkStatUpgradeInvalid(Context, Result, FText::Format(
				NSLOCTEXT("StatUpgradeDefinition", "NonFiniteValue", "{0} must be a finite value."),
				FieldName));
		}
	}

	void ValidateNonNegative(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const float Value,
		const FText& FieldName)
	{
		if (!IsFinite(Value) || Value < 0.f)
		{
			MarkStatUpgradeInvalid(Context, Result, FText::Format(
				NSLOCTEXT("StatUpgradeDefinition", "InvalidNonNegativeValue", "{0} must be a non-negative finite value."),
				FieldName));
		}
	}

	void ValidatePositive(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const float Value,
		const FText& FieldName)
	{
		if (!IsFinite(Value) || Value <= 0.f)
		{
			MarkStatUpgradeInvalid(Context, Result, FText::Format(
				NSLOCTEXT("StatUpgradeDefinition", "InvalidPositiveValue", "{0} must be a positive finite value."),
				FieldName));
		}
	}

	void ValidateLessOrEqual(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const float Value,
		const float MaxValue,
		const FText& FieldName)
	{
		if (IsFinite(Value) && Value > MaxValue)
		{
			MarkStatUpgradeInvalid(Context, Result, FText::Format(
				NSLOCTEXT("StatUpgradeDefinition", "ValueAboveMaximum", "{0} must be less than or equal to {1}."),
				FieldName,
				FText::AsNumber(MaxValue)));
		}
	}
#endif

	struct FDefaultStatLevelTagMapping
	{
		FGameplayTag StatTag;
		FGameplayTag LevelTag;
	};

	const FPdStatAttributeDefaultValue* FindExactAttributeValue(
		const TArray<FPdStatAttributeDefaultValue>& AttributeValues,
		const FGameplayTag& StatTag)
	{
		for (const FPdStatAttributeDefaultValue& AttributeValue : AttributeValues)
		{
			if (AttributeValue.StatTag.MatchesTagExact(StatTag))
			{
				return &AttributeValue;
			}
		}

		return nullptr;
	}

	const FPdStatAttributeDefaultValue* FindMatchingAttributeValue(
		const TArray<FPdStatAttributeDefaultValue>& AttributeValues,
		const FGameplayTag& StatTag)
	{
		if (const FPdStatAttributeDefaultValue* ExactValue = FindExactAttributeValue(AttributeValues, StatTag))
		{
			return ExactValue;
		}

		for (const FPdStatAttributeDefaultValue& AttributeValue : AttributeValues)
		{
			if (AttributeValue.StatTag.IsValid() && StatTag.MatchesTag(AttributeValue.StatTag))
			{
				return &AttributeValue;
			}
		}

		return nullptr;
	}

	const FPdStatUpgradeRule* FindMatchingUpgradeRule(
		const TArray<FPdStatUpgradeRule>& UpgradeRules,
		const FGameplayTag& StatTag)
	{
		for (const FPdStatUpgradeRule& Rule : UpgradeRules)
		{
			if (Rule.RootTag.IsValid() && StatTag.MatchesTag(Rule.RootTag))
			{
				return &Rule;
			}
		}

		return nullptr;
	}
}

UStatUpgradeDefinition::UStatUpgradeDefinition()
{
}

FPrimaryAssetId UStatUpgradeDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("StatUpgradeDefinition"), GetFName());
}

const FPdStatUpgradeRule* UStatUpgradeDefinition::FindUpgradeRuleForStat(const FGameplayTag& StatTag) const
{
	return StatTag.IsValid() ? FindMatchingUpgradeRule(UpgradeRules, StatTag) : nullptr;
}

float UStatUpgradeDefinition::GetMaxInvestedLevel() const
{
	return FMath::Clamp(MaxInvestedLevel, 0.f, MaxSupportedInvestedLevel);
}

bool UStatUpgradeDefinition::TryResolveDefaultStatLevelTag(const FGameplayTag& StatTag, FGameplayTag& OutLevelTag)
{
	OutLevelTag = FGameplayTag();
	if (!StatTag.IsValid())
	{
		return false;
	}

	const FDefaultStatLevelTagMapping DefaultMappings[] =
	{
		{ LabGameplayTags::Status_Offense_Strength, LabGameplayTags::Status_Offense_StrengthLevel },
		{ LabGameplayTags::Status_Offense_Intelligence, LabGameplayTags::Status_Offense_IntelligenceLevel },
		{ LabGameplayTags::Status_Offense_Critical, LabGameplayTags::Status_Offense_CriticalLevel },
		{ LabGameplayTags::Status_Defense_Armor, LabGameplayTags::Status_Defense_ArmorLevel },
		{ LabGameplayTags::Status_Defense_Recovery, LabGameplayTags::Status_Defense_RecoveryLevel },
		{ LabGameplayTags::Status_Defense_MaxShield, LabGameplayTags::Status_Defense_MaxShieldLevel },
		{ LabGameplayTags::Status_Defense_MaxShieldIncreasePercent, LabGameplayTags::Status_Defense_MaxShieldLevel },
		{ LabGameplayTags::Status_Resistance_Frostbite, LabGameplayTags::Status_Resistance_FrostbiteLevel },
		{ LabGameplayTags::Status_Resistance_Burn, LabGameplayTags::Status_Resistance_BurnLevel },
		{ LabGameplayTags::Status_Resistance_ElectricShock, LabGameplayTags::Status_Resistance_ElectricShockLevel },
		{ LabGameplayTags::Status_PandoraForce_FirstPandora, LabGameplayTags::Status_PandoraForce_FirstPandoraLevel },
		{ LabGameplayTags::Status_PandoraForce_SecondPandora, LabGameplayTags::Status_PandoraForce_SecondPandoraLevel },
		{ LabGameplayTags::Status_PandoraForce_ThirdPandora, LabGameplayTags::Status_PandoraForce_ThirdPandoraLevel },
		{ LabGameplayTags::Status_Resource_MaxHealth, LabGameplayTags::Status_Resource_MaxHealthLevel },
		{ LabGameplayTags::Status_Resource_MaxHealthIncreasePercent, LabGameplayTags::Status_Resource_MaxHealthLevel },
		{ LabGameplayTags::Status_Resource_MaxMana, LabGameplayTags::Status_Resource_MaxManaLevel },
		{ LabGameplayTags::Status_Resource_MaxManaIncreasePercent, LabGameplayTags::Status_Resource_MaxManaLevel },
		{ LabGameplayTags::Status_Resource_MaxStamina, LabGameplayTags::Status_Resource_MaxStaminaLevel },
		{ LabGameplayTags::Status_Resource_MaxStaminaIncreasePercent, LabGameplayTags::Status_Resource_MaxStaminaLevel },
		{ LabGameplayTags::Status_Agility_AttackSpeed, LabGameplayTags::Status_Agility_AttackSpeedLevel },
		{ LabGameplayTags::Status_Agility_MovementSpeed, LabGameplayTags::Status_Agility_MovementSpeedLevel },
		{ LabGameplayTags::Status_Agility_Arcane, LabGameplayTags::Status_Agility_ArcaneLevel }
	};

	for (const FDefaultStatLevelTagMapping& Mapping : DefaultMappings)
	{
		if (StatTag.MatchesTagExact(Mapping.StatTag))
		{
			OutLevelTag = Mapping.LevelTag;
			return OutLevelTag.IsValid();
		}
	}

	return false;
}

bool UStatUpgradeDefinition::TryGetAttributeValuePerUpgrade(const FGameplayTag& StatTag, float& OutValue) const
{
	if (!StatTag.IsValid())
	{
		return false;
	}

	for (const FPdStatAttributeDefaultValue& AttributeValue : AttributeDefaultValues)
	{
		if (AttributeValue.StatTag.MatchesTagExact(StatTag))
		{
			OutValue = AttributeValue.ValuePerUpgrade;
			return true;
		}
	}

	for (const FPdStatAttributeDefaultValue& AttributeValue : AttributeDefaultValues)
	{
		if (AttributeValue.StatTag.IsValid() && StatTag.MatchesTag(AttributeValue.StatTag))
		{
			OutValue = AttributeValue.ValuePerUpgrade;
			return true;
		}
	}

	return false;
}

float UStatUpgradeDefinition::GetAttributeValuePerUpgrade(const FGameplayTag& StatTag) const
{
	float Value = 1.f;
	if (TryGetAttributeValuePerUpgrade(StatTag, Value))
	{
		return Value;
	}

	UE_LOG(
		StatUpgradeDefinitionLog,
		Error,
		TEXT("[StatUpgrade] Missing ValuePerUpgrade for stat '%s' in '%s'. Falling back to 1."),
		*StatTag.ToString(),
		*GetPathName());

	return 1.f;
}

bool UStatUpgradeDefinition::TryGetExactAttributeDefaultValue(const FGameplayTag& StatTag, float& OutValue) const
{
	const FPdStatAttributeDefaultValue* AttributeValue = StatTag.IsValid()
		? FindExactAttributeValue(AttributeDefaultValues, StatTag)
		: nullptr;
	if (!AttributeValue)
	{
		return false;
	}

	OutValue = AttributeValue->DefaultValue;
	return true;
}

#if WITH_EDITOR
EDataValidationResult UStatUpgradeDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (Result == EDataValidationResult::NotValidated)
	{
		Result = EDataValidationResult::Valid;
	}

	if (!StatUpGameplayEffectClass)
	{
		MarkStatUpgradeInvalid(Context, Result, NSLOCTEXT("StatUpgradeDefinition", "MissingGameplayEffect", "StatUpGameplayEffectClass is required."));
	}

	ValidatePositive(Context, Result, MaxInvestedLevel, NSLOCTEXT("StatUpgradeDefinition", "MaxInvestedLevelField", "MaxInvestedLevel"));
	ValidateLessOrEqual(Context, Result, MaxInvestedLevel, MaxSupportedInvestedLevel, NSLOCTEXT("StatUpgradeDefinition", "MaxInvestedLevelField", "MaxInvestedLevel"));

	if (bEnableRecoveryHealthRegen)
	{
		if (!RecoveryHealGameplayEffectClass)
		{
			Context.AddWarning(NSLOCTEXT(
				"StatUpgradeDefinition",
				"MissingRecoveryHealGameplayEffect",
				"Recovery health regeneration is enabled, but RecoveryHealGameplayEffectClass is not set."));
		}
	}

	TSet<FGameplayTag> UpgradeRootTags;
	for (int32 EntryIndex = 0; EntryIndex < UpgradeRules.Num(); ++EntryIndex)
	{
		const FPdStatUpgradeRule& Rule = UpgradeRules[EntryIndex];
		if (!Rule.IsValid())
		{
			MarkStatUpgradeInvalid(Context, Result, FText::Format(
				NSLOCTEXT("StatUpgradeDefinition", "InvalidUpgradeRule", "UpgradeRules entry {0} requires RootTag."),
				FText::AsNumber(EntryIndex)));
			continue;
		}

		if (UpgradeRootTags.Contains(Rule.RootTag))
		{
			MarkStatUpgradeInvalid(Context, Result, FText::Format(
				NSLOCTEXT("StatUpgradeDefinition", "DuplicateUpgradeRule", "UpgradeRules entry {0} duplicates RootTag '{1}'."),
				FText::AsNumber(EntryIndex),
				FText::FromString(Rule.RootTag.ToString())));
			continue;
		}

		UpgradeRootTags.Add(Rule.RootTag);

		ValidateNonNegative(
			Context,
			Result,
			Rule.Cost,
			FText::Format(
				NSLOCTEXT("StatUpgradeDefinition", "UpgradeRuleCostField", "UpgradeRules entry {0} Cost"),
				FText::AsNumber(EntryIndex)));

		if (Rule.Cost > 0.f && !Rule.CostPointTag.IsValid())
		{
			MarkStatUpgradeInvalid(Context, Result, FText::Format(
				NSLOCTEXT("StatUpgradeDefinition", "MissingCostPointTag", "UpgradeRules entry {0} has Cost but no CostPointTag."),
				FText::AsNumber(EntryIndex)));
		}
		else if (Rule.Cost <= 0.f && Rule.CostPointTag.IsValid())
		{
			Context.AddWarning(FText::Format(
				NSLOCTEXT("StatUpgradeDefinition", "UnusedCostPointTag", "UpgradeRules entry {0} has CostPointTag, but Cost is zero."),
				FText::AsNumber(EntryIndex)));
		}

		float ValuePerUpgrade = 0.f;
		if (!TryGetAttributeValuePerUpgrade(Rule.RootTag, ValuePerUpgrade))
		{
			Context.AddWarning(FText::Format(
				NSLOCTEXT("StatUpgradeDefinition", "MissingValuePerUpgrade", "UpgradeRules entry {0} has no matching Attribute Values entry. Runtime will fall back to 1."),
				FText::AsNumber(EntryIndex)));
		}
		else if (FMath::IsNearlyZero(ValuePerUpgrade))
		{
			Context.AddWarning(FText::Format(
				NSLOCTEXT("StatUpgradeDefinition", "ZeroValuePerUpgrade", "UpgradeRules entry {0} resolves to zero ValuePerUpgrade. Upgrading this stat will have no stat magnitude."),
				FText::AsNumber(EntryIndex)));
		}
	}

	if (UpgradeRules.IsEmpty())
	{
		Context.AddWarning(NSLOCTEXT("StatUpgradeDefinition", "NoUpgradeRules", "StatUpgradeDefinition has no upgrade rules."));
	}

	TSet<FGameplayTag> PairedMaxStatTags;
	for (int32 EntryIndex = 0; EntryIndex < PairedResourceStatTags.Num(); ++EntryIndex)
	{
		const FPdPairedResourceStatTag& Pair = PairedResourceStatTags[EntryIndex];
		if (!Pair.IsValid())
		{
			MarkStatUpgradeInvalid(Context, Result, FText::Format(
				NSLOCTEXT("StatUpgradeDefinition", "InvalidPairedResource", "PairedResourceStatTags entry {0} requires both MaxStatTag and CurrentStatTag."),
				FText::AsNumber(EntryIndex)));
		}
		else if (Pair.MaxStatTag == Pair.CurrentStatTag)
		{
			MarkStatUpgradeInvalid(Context, Result, FText::Format(
				NSLOCTEXT("StatUpgradeDefinition", "SelfPairedResource", "PairedResourceStatTags entry {0} cannot use the same tag for max and current resource."),
				FText::AsNumber(EntryIndex)));
		}
		else if (PairedMaxStatTags.Contains(Pair.MaxStatTag))
		{
			MarkStatUpgradeInvalid(Context, Result, FText::Format(
				NSLOCTEXT("StatUpgradeDefinition", "DuplicatePairedResource", "PairedResourceStatTags entry {0} duplicates MaxStatTag '{1}'."),
				FText::AsNumber(EntryIndex),
				FText::FromString(Pair.MaxStatTag.ToString())));
		}
		else
		{
			PairedMaxStatTags.Add(Pair.MaxStatTag);
		}
	}

	TSet<FGameplayTag> AttributeDefaultTags;
	for (int32 EntryIndex = 0; EntryIndex < AttributeDefaultValues.Num(); ++EntryIndex)
	{
		const FPdStatAttributeDefaultValue& AttributeDefault = AttributeDefaultValues[EntryIndex];
		if (!AttributeDefault.IsValid())
		{
			MarkStatUpgradeInvalid(Context, Result, FText::Format(
				NSLOCTEXT("StatUpgradeDefinition", "InvalidAttributeValue", "Attribute Values entry {0} requires StatTag."),
				FText::AsNumber(EntryIndex)));
			continue;
		}

		if (AttributeDefaultTags.Contains(AttributeDefault.StatTag))
		{
			MarkStatUpgradeInvalid(Context, Result, FText::Format(
				NSLOCTEXT("StatUpgradeDefinition", "DuplicateAttributeValue", "Attribute Values entry {0} duplicates StatTag '{1}'."),
				FText::AsNumber(EntryIndex),
				FText::FromString(AttributeDefault.StatTag.ToString())));
			continue;
		}

		AttributeDefaultTags.Add(AttributeDefault.StatTag);
		ValidateFinite(
			Context,
			Result,
			AttributeDefault.DefaultValue,
			FText::Format(
				NSLOCTEXT("StatUpgradeDefinition", "AttributeDefaultValueField", "Attribute Values entry {0} DefaultValue"),
				FText::AsNumber(EntryIndex)));
		ValidateFinite(
			Context,
			Result,
			AttributeDefault.ValuePerUpgrade,
			FText::Format(
				NSLOCTEXT("StatUpgradeDefinition", "AttributeValuePerUpgradeField", "Attribute Values entry {0} ValuePerUpgrade"),
				FText::AsNumber(EntryIndex)));
	}

	return Result;
}
#endif
