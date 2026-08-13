#include "Definition/Level/LevelDefinition.h"

#include "Definition/Mode/PdGameInstanceDefinition.h"
#include "Misc/PackageName.h"
#include "UI/Widget/MapWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LevelDefinition)

namespace
{
	FString ResolveMapPackageName(const TSoftObjectPtr<UWorld>& Map)
	{
		return Map.ToSoftObjectPath().GetLongPackageName();
	}

	bool DoesMapMatchLevelName(
		const TSoftObjectPtr<UWorld>& Map,
		const FString& LevelName)
	{
		const FString MapPackageName = ResolveMapPackageName(Map);
		return !MapPackageName.IsEmpty()
			&& (MapPackageName.Equals(LevelName, ESearchCase::IgnoreCase)
				|| FPackageName::GetShortName(MapPackageName).Equals(
					LevelName,
					ESearchCase::IgnoreCase));
	}

#if WITH_EDITOR
	void MarkLevelDefinitionInvalid(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const FText& Message)
	{
		Result = EDataValidationResult::Invalid;
		Context.AddError(Message);
	}

	void ValidateRequiredLevel(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const TSoftObjectPtr<UWorld>& Level,
		const TCHAR* FieldName)
	{
		if (Level.IsNull())
		{
			MarkLevelDefinitionInvalid(Context, Result, FText::Format(
				NSLOCTEXT("LevelDefinition", "MissingRequiredLevel", "{0} is required."),
				FText::FromString(FieldName)));
		}
	}

	void ValidateIngameLevels(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const TArray<FLobbyMatchMapOption>& IngameLevels)
	{
		if (IngameLevels.IsEmpty())
		{
			MarkLevelDefinitionInvalid(Context, Result, NSLOCTEXT(
				"LevelDefinition",
				"EmptyIngameLevels",
				"IngameLevels must contain at least one level."));
			return;
		}

		TSet<FName> UsedLevelKeys;
		for (int32 Index = 0; Index < IngameLevels.Num(); ++Index)
		{
			const FLobbyMatchMapOption& Level = IngameLevels[Index];
			const FText LevelLabel = FText::Format(
				NSLOCTEXT("LevelDefinition", "IngameLevelLabel", "IngameLevels[{0}] ({1})"),
				FText::AsNumber(Index),
				FText::FromName(Level.MapKey));

			if (Level.MapKey.IsNone())
			{
				MarkLevelDefinitionInvalid(Context, Result, FText::Format(
					NSLOCTEXT("LevelDefinition", "MissingLevelKey", "{0} MapKey is required."),
					LevelLabel));
			}
			else if (UsedLevelKeys.Contains(Level.MapKey))
			{
				MarkLevelDefinitionInvalid(Context, Result, FText::Format(
					NSLOCTEXT("LevelDefinition", "DuplicateLevelKey", "{0} has a duplicate MapKey."),
					LevelLabel));
			}
			UsedLevelKeys.Add(Level.MapKey);

			if (Level.DisplayName.IsEmpty())
			{
				Context.AddWarning(FText::Format(
					NSLOCTEXT("LevelDefinition", "MissingDisplayName", "{0} DisplayName is empty."),
					LevelLabel));
			}

			if (Level.Map.IsNull() && Level.TravelMapName.TrimStartAndEnd().IsEmpty())
			{
				MarkLevelDefinitionInvalid(Context, Result, FText::Format(
					NSLOCTEXT("LevelDefinition", "MissingTravelDestination", "{0} must set Map or TravelMapName."),
					LevelLabel));
			}
			if (!Level.Thumbnail)
			{
				MarkLevelDefinitionInvalid(Context, Result, FText::Format(
					NSLOCTEXT("LevelDefinition", "MissingThumbnail", "{0} Thumbnail is required."),
					LevelLabel));
			}
			if (Level.MaxPlayerCount < 1 || Level.MaxPlayerCount > LabGameSession::MaxPlayerCount)
			{
				MarkLevelDefinitionInvalid(Context, Result, FText::Format(
					NSLOCTEXT("LevelDefinition", "InvalidMaxPlayerCount", "{0} MaxPlayerCount must be between 1 and the session limit."),
					LevelLabel));
			}
			if (!Level.GameplayMapWidgetClass.IsNull()
				&& !Level.GameplayMapWidgetClass.LoadSynchronous())
			{
				MarkLevelDefinitionInvalid(Context, Result, FText::Format(
					NSLOCTEXT("LevelDefinition", "InvalidGameplayMapWidgetClass", "{0} GameplayMapWidgetClass could not be loaded: {1}"),
					LevelLabel,
					FText::FromString(Level.GameplayMapWidgetClass.ToString())));
			}
		}
	}
#endif
}

FPrimaryAssetId ULevelDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("LevelDefinition"), GetFName());
}

FSoftObjectPath ULevelDefinition::GetDefaultDefinitionPath()
{
	return UPdGameInstanceDefinition::GetConfiguredDefinitionReferences()
		.LevelDefinition.ToSoftObjectPath();
}

const ULevelDefinition* ULevelDefinition::ResolveDefaultDefinition()
{
	const FSoftObjectPath DefinitionPath = GetDefaultDefinitionPath();
	if (!DefinitionPath.IsValid())
	{
		return nullptr;
	}
	if (const ULevelDefinition* LoadedDefinition =
		Cast<ULevelDefinition>(DefinitionPath.ResolveObject()))
	{
		return LoadedDefinition;
	}
	return Cast<ULevelDefinition>(DefinitionPath.TryLoad());
}

bool ULevelDefinition::GetIngameLevelAtIndex(
	const int32 Index,
	FLobbyMatchMapOption& OutLevel) const
{
	if (!IngameLevels.IsValidIndex(Index))
	{
		return false;
	}
	OutLevel = IngameLevels[Index];
	return true;
}

bool ULevelDefinition::FindIngameLevel(
	const FName LevelKey,
	FLobbyMatchMapOption& OutLevel) const
{
	const FName ResolvedLevelKey = ResolveIngameLevelKey(LevelKey);
	if (ResolvedLevelKey.IsNone())
	{
		return false;
	}
	for (const FLobbyMatchMapOption& Level : IngameLevels)
	{
		if (Level.MapKey == ResolvedLevelKey)
		{
			OutLevel = Level;
			return true;
		}
	}
	return false;
}

FName ULevelDefinition::ResolveIngameLevelKey(const FName LevelKey) const
{
	if (IngameLevels.IsEmpty())
	{
		return LevelKey;
	}
	if (!LevelKey.IsNone())
	{
		for (const FLobbyMatchMapOption& Level : IngameLevels)
		{
			if (Level.MapKey == LevelKey)
			{
				return LevelKey;
			}
		}
	}
	return IngameLevels[0].MapKey;
}

FString ULevelDefinition::GetTitleTravelMapName() const
{
	return ResolveMapPackageName(TitleLevel);
}

FString ULevelDefinition::GetLobbyTravelMapName() const
{
	return ResolveMapPackageName(LobbyLevel);
}

FString ULevelDefinition::GetRoomTravelMapName() const
{
	return ResolveMapPackageName(RoomLevel);
}

FString ULevelDefinition::GetTrainingRoomTravelMapName() const
{
	return ResolveMapPackageName(TrainingLevel);
}

bool ULevelDefinition::IsLobbyMapName(const FString& LevelName) const
{
	return !LevelName.TrimStartAndEnd().IsEmpty()
		&& DoesMapMatchLevelName(LobbyLevel, LevelName);
}

bool ULevelDefinition::IsTrainingRoomMapName(const FString& LevelName) const
{
	return !LevelName.TrimStartAndEnd().IsEmpty()
		&& DoesMapMatchLevelName(TrainingLevel, LevelName);
}

#if WITH_EDITOR
EDataValidationResult ULevelDefinition::IsDataValid(
	FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (Result == EDataValidationResult::NotValidated)
	{
		Result = EDataValidationResult::Valid;
	}

	ValidateIngameLevels(Context, Result, IngameLevels);
	ValidateRequiredLevel(Context, Result, TitleLevel, TEXT("TitleLevel"));
	ValidateRequiredLevel(Context, Result, LobbyLevel, TEXT("LobbyLevel"));
	ValidateRequiredLevel(Context, Result, RoomLevel, TEXT("RoomLevel"));
	ValidateRequiredLevel(Context, Result, TrainingLevel, TEXT("TrainingLevel"));
	return Result;
}
#endif
