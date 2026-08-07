#include "Item/ItemInstance.h"

#include "Definition/Item/ItemDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ItemInstance)

DEFINE_LOG_CATEGORY(ItemInstanceLog);

FGuid UItemInstance::GetOrCreateItemId()
{
	EnsureItemId();
	return ItemId;
}

void UItemInstance::EnsureItemId()
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	if (!ItemId.IsValid())
	{
		ItemId = FGuid::NewGuid();
	}
}

float UItemInstance::GetUpgradeBonusStatMagnitude(const FGameplayTag StatTag) const
{
	if (!StatTag.IsValid() || GetUpgradeLevel() <= 0)
	{
		return 0.0f;
	}

	float UnupgradedMagnitude = 0.0f;
	if (const UItemDefinition* Definition = ItemDefinition.Get())
	{
		UnupgradedMagnitude += Definition->Map_Stat_Magnitude.FindRef(StatTag);
	}
	UnupgradedMagnitude += Map_EnhancedStat_Magnitude.FindRef(StatTag);

	const double BonusMagnitude = static_cast<double>(UnupgradedMagnitude)
		* static_cast<double>(GetUpgradeLevel())
		* static_cast<double>(UpgradeStatBonusRatePerLevel);
	if (!FMath::IsFinite(BonusMagnitude))
	{
		return 0.0f;
	}

	const double MaxFloatMagnitude = static_cast<double>(TNumericLimits<float>::Max());
	return static_cast<float>(FMath::Clamp(
		BonusMagnitude,
		-MaxFloatMagnitude,
		MaxFloatMagnitude));
}

float UItemInstance::GetEffectiveStatMagnitude(const FGameplayTag StatTag) const
{
	if (!StatTag.IsValid())
	{
		return 0.0f;
	}

	float Result = 0.0f;
	if (const UItemDefinition* Definition = ItemDefinition.Get())
	{
		Result += Definition->Map_Stat_Magnitude.FindRef(StatTag);
	}
	Result += Map_EnhancedStat_Magnitude.FindRef(StatTag);
	Result += GetUpgradeBonusStatMagnitude(StatTag);
	return FMath::IsFinite(Result) ? Result : 0.0f;
}

void UItemInstance::BuildUpgradeBonusStatMagnitudes(
	TMap<FGameplayTag, float>& OutMagnitudes) const
{
	OutMagnitudes.Reset();
	if (GetUpgradeLevel() <= 0)
	{
		return;
	}

	TSet<FGameplayTag> StatTags;
	if (const UItemDefinition* Definition = ItemDefinition.Get())
	{
		for (const TPair<FGameplayTag, float>& Pair : Definition->Map_Stat_Magnitude)
		{
			if (Pair.Key.IsValid())
			{
				StatTags.Add(Pair.Key);
			}
		}
	}
	for (const TPair<FGameplayTag, float>& Pair : Map_EnhancedStat_Magnitude)
	{
		if (Pair.Key.IsValid())
		{
			StatTags.Add(Pair.Key);
		}
	}

	for (const FGameplayTag& StatTag : StatTags)
	{
		const float BonusMagnitude = GetUpgradeBonusStatMagnitude(StatTag);
		if (!FMath::IsNearlyZero(BonusMagnitude))
		{
			OutMagnitudes.Add(StatTag, BonusMagnitude);
		}
	}
}

void UItemInstance::BuildEffectiveStatMagnitudes(
	TMap<FGameplayTag, float>& OutMagnitudes) const
{
	OutMagnitudes.Reset();
	if (const UItemDefinition* Definition = ItemDefinition.Get())
	{
		OutMagnitudes = Definition->Map_Stat_Magnitude;
	}

	for (const TPair<FGameplayTag, float>& Pair : Map_EnhancedStat_Magnitude)
	{
		if (Pair.Key.IsValid() && FMath::IsFinite(Pair.Value))
		{
			OutMagnitudes.FindOrAdd(Pair.Key) += Pair.Value;
		}
	}

	TMap<FGameplayTag, float> UpgradeBonusMagnitudes;
	BuildUpgradeBonusStatMagnitudes(UpgradeBonusMagnitudes);
	for (const TPair<FGameplayTag, float>& Pair : UpgradeBonusMagnitudes)
	{
		OutMagnitudes.FindOrAdd(Pair.Key) += Pair.Value;
	}
}

void UItemInstance::SetUpgradeLevel(const int32 InUpgradeLevel)
{
	UpgradeLevel = FMath::Max(InUpgradeLevel, 0);
}
