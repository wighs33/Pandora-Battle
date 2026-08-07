#include "Definition/Match/MatchRuleDefinition.h"

#include "Definition/Mode/PdGameInstanceDefinition.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "UI/Widget/MapWidget.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(MatchRuleDefinition)

namespace
{
#if WITH_EDITOR
	void MarkMatchRuleInvalid(FDataValidationContext& Context, EDataValidationResult& Result, const FText& Message)
	{
		Result = EDataValidationResult::Invalid;
		Context.AddError(Message);
	}

	FText MatchRuleFieldText(const TCHAR* FieldName)
	{
		return FText::FromString(FString(FieldName));
	}

	void ValidateFiniteNonNegativeFloat(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const float Value,
		const TCHAR* FieldName)
	{
		if (!FMath::IsFinite(Value) || Value < 0.0f)
		{
			MarkMatchRuleInvalid(Context, Result, FText::Format(
				NSLOCTEXT("MatchRuleDefinition", "InvalidNonNegativeFloat", "{0} must be a non-negative finite value."),
				MatchRuleFieldText(FieldName)));
		}
	}

	void ValidateFinitePositiveFloat(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const float Value,
		const TCHAR* FieldName)
	{
		if (!FMath::IsFinite(Value) || Value <= 0.0f)
		{
			MarkMatchRuleInvalid(Context, Result, FText::Format(
				NSLOCTEXT("MatchRuleDefinition", "InvalidPositiveFloat", "{0} must be a positive finite value."),
				MatchRuleFieldText(FieldName)));
		}
	}

	bool HasLobbyTravelDestination(const FLobbyMatchMapOption& MapOption)
	{
		return !MapOption.Map.IsNull() || !MapOption.TravelMapName.TrimStartAndEnd().IsEmpty();
	}

	FText MakeLobbyMapOptionLabel(const int32 Index, const FLobbyMatchMapOption& MapOption)
	{
		return FText::Format(
			NSLOCTEXT("MatchRuleDefinition", "LobbyMapOptionLabel", "LobbyMapOptions[{0}] ({1})"),
			FText::AsNumber(Index),
			FText::FromName(MapOption.MapKey));
	}

	void ValidateLobbyMapOptions(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const TArray<FLobbyMatchMapOption>& LobbyMapOptions)
	{
		if (LobbyMapOptions.IsEmpty())
		{
			Context.AddWarning(NSLOCTEXT(
				"MatchRuleDefinition",
				"EmptyLobbyMapOptions",
				"LobbyMapOptions is empty. Lobby map selection will fall back to the requested map key."));
			return;
		}

		TSet<FName> UsedMapKeys;
		for (int32 Index = 0; Index < LobbyMapOptions.Num(); ++Index)
		{
			const FLobbyMatchMapOption& MapOption = LobbyMapOptions[Index];
			const FText OptionLabel = MakeLobbyMapOptionLabel(Index, MapOption);

			if (MapOption.MapKey.IsNone())
			{
				MarkMatchRuleInvalid(Context, Result, FText::Format(
					NSLOCTEXT("MatchRuleDefinition", "MissingMapKey", "{0} MapKey is required."),
					OptionLabel));
			}
			else if (UsedMapKeys.Contains(MapOption.MapKey))
			{
				MarkMatchRuleInvalid(Context, Result, FText::Format(
					NSLOCTEXT("MatchRuleDefinition", "DuplicateMapKey", "{0} has a duplicate MapKey."),
					OptionLabel));
			}
			UsedMapKeys.Add(MapOption.MapKey);

			if (MapOption.DisplayName.IsEmpty())
			{
				Context.AddWarning(FText::Format(
					NSLOCTEXT("MatchRuleDefinition", "MissingDisplayName", "{0} DisplayName is empty."),
					OptionLabel));
			}

			if (!HasLobbyTravelDestination(MapOption))
			{
				MarkMatchRuleInvalid(Context, Result, FText::Format(
					NSLOCTEXT("MatchRuleDefinition", "MissingTravelDestination", "{0} must set Map or TravelMapName."),
					OptionLabel));
			}

			if (!MapOption.Thumbnail)
			{
				MarkMatchRuleInvalid(Context, Result, FText::Format(
					NSLOCTEXT("MatchRuleDefinition", "MissingLobbyMapThumbnail", "{0} Thumbnail is required before the lobby can be presented."),
					OptionLabel));
			}

			if (MapOption.MaxPlayerCount < 1)
			{
				MarkMatchRuleInvalid(Context, Result, FText::Format(
					NSLOCTEXT("MatchRuleDefinition", "InvalidMaxPlayerCount", "{0} MaxPlayerCount must be at least 1."),
					OptionLabel));
			}
			else if (MapOption.MaxPlayerCount > LabGameSession::MaxPlayerCount)
			{
				MarkMatchRuleInvalid(Context, Result, FText::Format(
					NSLOCTEXT("MatchRuleDefinition", "MaxPlayerCountExceedsSessionLimit", "{0} MaxPlayerCount cannot exceed LabGameSession::MaxPlayerCount."),
					OptionLabel));
			}

			if (!MapOption.GameplayMapWidgetClass.IsNull() && !MapOption.GameplayMapWidgetClass.LoadSynchronous())
			{
				MarkMatchRuleInvalid(Context, Result, FText::Format(
					NSLOCTEXT("MatchRuleDefinition", "InvalidGameplayMapWidgetClass", "{0} GameplayMapWidgetClass could not be loaded: {1}"),
					OptionLabel,
					FText::FromString(MapOption.GameplayMapWidgetClass.ToString())));
			}
		}
	}

	void ValidateTeamOverlayMaterials(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const TArray<FTeamOverlayMaterial>& TeamOverlayMaterials)
	{
		TSet<uint8> UsedTeamColors;
		for (int32 Index = 0; Index < TeamOverlayMaterials.Num(); ++Index)
		{
			const FTeamOverlayMaterial& TeamOverlayMaterial = TeamOverlayMaterials[Index];
			const uint8 TeamColorValue = static_cast<uint8>(TeamOverlayMaterial.TeamColor);

			if (UsedTeamColors.Contains(TeamColorValue))
			{
				MarkMatchRuleInvalid(Context, Result, FText::Format(
					NSLOCTEXT("MatchRuleDefinition", "DuplicateTeamOverlayColor", "TeamOverlayMaterials[{0}] has a duplicate TeamColor."),
					FText::AsNumber(Index)));
			}
			UsedTeamColors.Add(TeamColorValue);

			if (!TeamOverlayMaterial.OverlayMaterial)
			{
				Context.AddWarning(FText::Format(
					NSLOCTEXT("MatchRuleDefinition", "MissingTeamOverlayMaterial", "TeamOverlayMaterials[{0}] OverlayMaterial is not set."),
					FText::AsNumber(Index)));
			}
		}
	}

	void ValidateNameArray(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const TArray<FName>& Names,
		const TCHAR* FieldName)
	{
		TSet<FName> UsedNames;
		for (int32 Index = 0; Index < Names.Num(); ++Index)
		{
			const FName Name = Names[Index];
			if (Name.IsNone())
			{
				MarkMatchRuleInvalid(Context, Result, FText::Format(
					NSLOCTEXT("MatchRuleDefinition", "InvalidNameEntry", "{0}[{1}] must not be None."),
					MatchRuleFieldText(FieldName),
					FText::AsNumber(Index)));
				continue;
			}

			if (UsedNames.Contains(Name))
			{
				Context.AddWarning(FText::Format(
					NSLOCTEXT("MatchRuleDefinition", "DuplicateNameEntry", "{0} contains a duplicate entry: {1}"),
					MatchRuleFieldText(FieldName),
					FText::FromName(Name)));
			}
			UsedNames.Add(Name);
		}
	}
#endif
}

UMatchRuleDefinition::UMatchRuleDefinition()
{
}

FPrimaryAssetId UMatchRuleDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("MatchRuleDefinition"), GetFName());
}

FSoftObjectPath UMatchRuleDefinition::GetDefaultDefinitionPath()
{
	return UPdGameInstanceDefinition::GetConfiguredDefinitionReferences()
		.MatchRule.ToSoftObjectPath();
}

const UMatchRuleDefinition* UMatchRuleDefinition::ResolveDefaultDefinition()
{
	const FSoftObjectPath DefinitionPath = GetDefaultDefinitionPath();
	if (!DefinitionPath.IsValid())
	{
		return nullptr;
	}

	if (const UMatchRuleDefinition* LoadedDefinition =
		Cast<UMatchRuleDefinition>(DefinitionPath.ResolveObject()))
	{
		return LoadedDefinition;
	}

	return Cast<UMatchRuleDefinition>(DefinitionPath.TryLoad());
}

FString UMatchRuleDefinition::GetTrainingRoomTravelMapName() const
{
	return TrainingRoomMap.ToSoftObjectPath().GetLongPackageName();
}

bool UMatchRuleDefinition::IsTrainingRoomMapName(
	const FString& LevelName) const
{
	if (LevelName.TrimStartAndEnd().IsEmpty())
	{
		return false;
	}

	const FString MapPackageName = GetTrainingRoomTravelMapName();
	return !MapPackageName.IsEmpty()
		&& (MapPackageName.Equals(LevelName, ESearchCase::IgnoreCase)
			|| FPackageName::GetShortName(MapPackageName).Equals(
				LevelName,
				ESearchCase::IgnoreCase));
}

UMaterialInterface* UMatchRuleDefinition::GetTeamOverlayMaterial(const int32 TeamColorIndex) const
{
	ETeamColor TeamColor = ETeamColor::Red;
	if (!TryGetTeamColorForIndex(TeamColorIndex, TeamColor))
	{
		return nullptr;
	}

	return GetTeamOverlayMaterialByTeamColor(TeamColor);
}

bool UMatchRuleDefinition::TryGetTeamColorForIndex(const int32 TeamColorIndex, ETeamColor& OutTeamColor)
{
	switch (TeamColorIndex)
	{
	case 0:
		OutTeamColor = ETeamColor::Red;
		return true;
	case 1:
		OutTeamColor = ETeamColor::Blue;
		return true;
	case 2:
		OutTeamColor = ETeamColor::Yellow;
		return true;
	case 3:
		OutTeamColor = ETeamColor::Purple;
		return true;
	case 4:
		OutTeamColor = ETeamColor::Green;
		return true;
	case 5:
		OutTeamColor = ETeamColor::Orange;
		return true;
	default:
		OutTeamColor = ETeamColor::Red;
		return false;
	}
}

UMaterialInterface* UMatchRuleDefinition::GetTeamOverlayMaterialByTeamColor(const ETeamColor TeamColor) const
{
	for (const FTeamOverlayMaterial& TeamOverlayMaterial : TeamOverlayMaterials)
	{
		if (TeamOverlayMaterial.TeamColor == TeamColor)
		{
			return TeamOverlayMaterial.OverlayMaterial;
		}
	}

	return nullptr;
}

bool UMatchRuleDefinition::GetLobbyMapOptionAtIndex(const int32 Index, FLobbyMatchMapOption& OutMapOption) const
{
	if (!LobbyMapOptions.IsValidIndex(Index))
	{
		return false;
	}

	OutMapOption = LobbyMapOptions[Index];
	return true;
}

bool UMatchRuleDefinition::FindLobbyMapOption(const FName MapKey, FLobbyMatchMapOption& OutMapOption) const
{
	const FName ResolvedMapKey = ResolveLobbyMapKey(MapKey);
	if (ResolvedMapKey.IsNone())
	{
		return false;
	}

	for (const FLobbyMatchMapOption& MapOption : LobbyMapOptions)
	{
		if (MapOption.MapKey == ResolvedMapKey)
		{
			OutMapOption = MapOption;
			return true;
		}
	}

	return false;
}

FName UMatchRuleDefinition::ResolveLobbyMapKey(const FName MapKey) const
{
	if (LobbyMapOptions.IsEmpty())
	{
		return MapKey;
	}

	if (!MapKey.IsNone())
	{
		for (const FLobbyMatchMapOption& MapOption : LobbyMapOptions)
		{
			if (MapOption.MapKey == MapKey)
			{
				return MapKey;
			}
		}
	}

	return LobbyMapOptions[0].MapKey;
}

#if WITH_EDITOR
EDataValidationResult UMatchRuleDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (Result == EDataValidationResult::NotValidated)
	{
		Result = EDataValidationResult::Valid;
	}

	ValidateLobbyMapOptions(Context, Result, LobbyMapOptions);
	ValidateTeamOverlayMaterials(Context, Result, TeamOverlayMaterials);
	ValidateFiniteNonNegativeFloat(
		Context,
		Result,
		LobbyStartCountdownSeconds,
		TEXT("LobbyStartCountdownSeconds"));
	ValidateFiniteNonNegativeFloat(Context, Result, MatchTimerSeconds, TEXT("MatchTimerSeconds"));
	ValidateFiniteNonNegativeFloat(Context, Result, PlayerRespawnDelay, TEXT("PlayerRespawnDelay"));
	ValidateFinitePositiveFloat(Context, Result, HudTickInterval, TEXT("HudTickInterval"));
	ValidateNameArray(Context, Result, MapsWithoutMatchTimer, TEXT("MapsWithoutMatchTimer"));
	ValidateNameArray(Context, Result, RandomRespawnPlayerStartTags, TEXT("RandomRespawnPlayerStartTags"));

	if (MatchTimerSeconds <= 0.0f)
	{
		Context.AddWarning(NSLOCTEXT(
			"MatchRuleDefinition",
			"ZeroServerTimer",
			"MatchTimerSeconds is zero. The server match timer will expire immediately."));
	}

	if (bUseRandomPlayerStartRespawns && RandomRespawnPlayerStartTags.IsEmpty())
	{
		Context.AddWarning(NSLOCTEXT(
			"MatchRuleDefinition",
			"RandomRespawnWithoutTags",
			"bUseRandomPlayerStartRespawns is true, but RandomRespawnPlayerStartTags is empty. Respawn will fall back to the initial spawn transform."));
	}

	return Result;
}
#endif
