#include "PlayerComponent/StatUpgradeDefinition.h"

#include "GameplayEffect.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(StatUpgradeDefinition)

UStatUpgradeDefinition::UStatUpgradeDefinition()
{
}

FPrimaryAssetId UStatUpgradeDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("StatUpgradeDefinition"), GetFName());
}

#if WITH_EDITOR
EDataValidationResult UStatUpgradeDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (!StatUpGameplayEffectClass)
	{
		Result = EDataValidationResult::Invalid;
		Context.AddError(NSLOCTEXT("StatUpgradeDefinition", "MissingGameplayEffect", "StatUpGameplayEffectClass is required."));
	}

	TSet<FGameplayTag> UpgradeRootTags;
	for (int32 EntryIndex = 0; EntryIndex < UpgradeRules.Num(); ++EntryIndex)
	{
		const FPdStatUpgradeRule& Rule = UpgradeRules[EntryIndex];
		if (!Rule.IsValid())
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(
				NSLOCTEXT("StatUpgradeDefinition", "InvalidUpgradeRule", "UpgradeRules entry {0} requires RootTag and non-zero Magnitude."),
				FText::AsNumber(EntryIndex)));
			continue;
		}

		if (UpgradeRootTags.Contains(Rule.RootTag))
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(
				NSLOCTEXT("StatUpgradeDefinition", "DuplicateUpgradeRule", "UpgradeRules entry {0} duplicates RootTag '{1}'."),
				FText::AsNumber(EntryIndex),
				FText::FromString(Rule.RootTag.ToString())));
			continue;
		}

		UpgradeRootTags.Add(Rule.RootTag);
	}

	if (UpgradeRules.IsEmpty())
	{
		Context.AddWarning(NSLOCTEXT("StatUpgradeDefinition", "NoUpgradeRules", "StatUpgradeDefinition has no upgrade rules."));
	}

	for (int32 EntryIndex = 0; EntryIndex < PairedResourceStatTags.Num(); ++EntryIndex)
	{
		if (!PairedResourceStatTags[EntryIndex].IsValid())
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(
				NSLOCTEXT("StatUpgradeDefinition", "InvalidPairedResource", "PairedResourceStatTags entry {0} requires both MaxStatTag and CurrentStatTag."),
				FText::AsNumber(EntryIndex)));
		}
	}

	return Result;
}
#endif
