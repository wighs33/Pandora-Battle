#include "Definition/Match/MatchRuleDefinition.h"

#include "Definition/Mode/PdGameInstanceDefinition.h"
#include "Materials/MaterialInterface.h"

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

#if WITH_EDITOR
EDataValidationResult UMatchRuleDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (Result == EDataValidationResult::NotValidated)
	{
		Result = EDataValidationResult::Valid;
	}

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
