#include "Definition/Experience/ExperienceDefinition.h"

#if WITH_EDITOR
#include "GameFeaturesSubsystem.h"
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(ExperienceDefinition)

UExperienceDefinition::UExperienceDefinition()
{
}

FPrimaryAssetId UExperienceDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("ExperienceDefinition"), GetFName());
}

#if WITH_EDITOR
EDataValidationResult UExperienceDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (!DefaultPawnClass)
	{
		Result = EDataValidationResult::Invalid;
		Context.AddError(NSLOCTEXT("ExperienceDefinition", "MissingDefaultPawnClass", "DefaultPawnClass is required."));
	}

	TSet<FPrimaryAssetId> UniqueGameFeatureIds;
	for (int32 Index = 0; Index < GameFeaturesToEnable.Num(); ++Index)
	{
		const FPrimaryAssetId& GameFeatureId = GameFeaturesToEnable[Index];
		if (!GameFeatureId.IsValid())
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(
				NSLOCTEXT("ExperienceDefinition", "InvalidGameFeatureId", "GameFeaturesToEnable[{0}] is invalid."),
				FText::AsNumber(Index)));
			continue;
		}

		if (GameFeatureId.PrimaryAssetType != FPrimaryAssetType(TEXT("GameFeatureData")))
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(
				NSLOCTEXT("ExperienceDefinition", "InvalidGameFeatureType", "GameFeaturesToEnable[{0}] must be a GameFeatureData asset id. Current id: {1}"),
				FText::AsNumber(Index),
				FText::FromString(GameFeatureId.ToString())));
			continue;
		}

		if (UniqueGameFeatureIds.Contains(GameFeatureId))
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(
				NSLOCTEXT("ExperienceDefinition", "DuplicateGameFeatureId", "GameFeaturesToEnable contains duplicate GameFeatureData id: {0}"),
				FText::FromString(GameFeatureId.ToString())));
			continue;
		}
		UniqueGameFeatureIds.Add(GameFeatureId);

		FString PluginURL;
		if (!UGameFeaturesSubsystem::Get().GetPluginURLByName(GameFeatureId.PrimaryAssetName.ToString(), PluginURL))
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(
				NSLOCTEXT("ExperienceDefinition", "MissingGameFeaturePlugin", "GameFeature plugin could not be found for id: {0}"),
				FText::FromString(GameFeatureId.ToString())));
		}
	}

	if (GameFeaturesToEnable.IsEmpty())
	{
		Context.AddWarning(NSLOCTEXT("ExperienceDefinition", "NoGameFeatures", "ExperienceDefinition has no GameFeaturesToEnable entries."));
	}

	return Result;
}
#endif
