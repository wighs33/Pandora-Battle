#include "Definition/Mode/PdGameInstanceDefinition.h"

#include "SavedGameData/PdSaveGame.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PdGameInstanceDefinition)

namespace
{
	constexpr const TCHAR* DefaultGameInstanceDefinitionPath =
		TEXT("/Game/Data/DA_GameInstance.DA_GameInstance");
}

UPdGameInstanceDefinition::UPdGameInstanceDefinition()
{
	ProfilePersistence.SaveGameClass = UPdSaveGame::StaticClass();
}

FPrimaryAssetId UPdGameInstanceDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("GameInstanceDefinition"), GetFName());
}

FSoftObjectPath UPdGameInstanceDefinition::GetDefaultDefinitionPath()
{
	return FSoftObjectPath(DefaultGameInstanceDefinitionPath);
}

#if WITH_EDITOR
EDataValidationResult UPdGameInstanceDefinition::IsDataValid(
	FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (Result == EDataValidationResult::NotValidated)
	{
		Result = EDataValidationResult::Valid;
	}

	auto MarkInvalid = [&Context, &Result](const FText& Message)
	{
		Result = EDataValidationResult::Invalid;
		Context.AddError(Message);
	};

	if (!ProfilePersistence.SaveGameClass)
	{
		MarkInvalid(NSLOCTEXT(
			"PdGameInstanceDefinition",
			"MissingSaveGameClass",
			"ProfilePersistence SaveGameClass is required."));
	}

	if (!FMath::IsFinite(ProfilePersistence.SaveDebounceSeconds)
		|| ProfilePersistence.SaveDebounceSeconds < 0.05f)
	{
		MarkInvalid(NSLOCTEXT(
			"PdGameInstanceDefinition",
			"InvalidSaveDebounce",
			"ProfilePersistence SaveDebounceSeconds must be finite and at least 0.05 seconds."));
	}

	if (!FMath::IsFinite(ProfilePersistence.SaveTickerIntervalSeconds)
		|| ProfilePersistence.SaveTickerIntervalSeconds < 0.05f)
	{
		MarkInvalid(NSLOCTEXT(
			"PdGameInstanceDefinition",
			"InvalidSaveTickerInterval",
			"ProfilePersistence SaveTickerIntervalSeconds must be finite and at least 0.05 seconds."));
	}

	if (!FMath::IsFinite(ProfilePersistence.SaveRetryDelaySeconds)
		|| ProfilePersistence.SaveRetryDelaySeconds < 0.1f)
	{
		MarkInvalid(NSLOCTEXT(
			"PdGameInstanceDefinition",
			"InvalidSaveRetryDelay",
			"ProfilePersistence SaveRetryDelaySeconds must be finite and at least 0.1 seconds."));
	}

	if (ProfilePersistence.MaxSaveRetryAttempts < 0)
	{
		MarkInvalid(NSLOCTEXT(
			"PdGameInstanceDefinition",
			"InvalidSaveRetryCount",
			"ProfilePersistence MaxSaveRetryAttempts must be non-negative."));
	}

	return Result;
}
#endif
