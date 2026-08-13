#include "Experience/PdWorldSettings.h"

#if WITH_EDITOR
#include "Engine/World.h"
#include "GameMapsSettings.h"
#include "Lobby/Contents/LobbyGameMode.h"
#include "Mode/ExperienceGameMode.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(PdWorldSettings)

#if WITH_EDITOR
namespace PdWorldSettings
{
	UClass* ResolveEffectiveGameModeClass(const APdWorldSettings& WorldSettings)
	{
		if (WorldSettings.DefaultGameMode)
		{
			return WorldSettings.DefaultGameMode.Get();
		}

		const FString GlobalDefaultGameMode =
			UGameMapsSettings::GetGlobalDefaultGameMode();
		return GlobalDefaultGameMode.IsEmpty()
			? nullptr
			: LoadClass<AGameModeBase>(
				nullptr,
				*GlobalDefaultGameMode);
	}

	bool UsesProjectMapConvention(const APdWorldSettings& WorldSettings)
	{
		const UWorld* World = WorldSettings.GetWorld();
		const FString PackageName = World
			? World->GetOutermost()->GetName()
			: FString();
		return PackageName.StartsWith(TEXT("/Game/Map/LV_"));
	}

	bool RequiresExperience(
		const APdWorldSettings& WorldSettings,
		const UClass* GameModeClass)
	{
		const bool bIsProjectGameMap =
			WorldSettings.DefaultGameMode
			|| UsesProjectMapConvention(WorldSettings);
		return bIsProjectGameMap
			&& GameModeClass
			&& (GameModeClass->IsChildOf(
					AExperienceGameMode::StaticClass())
				|| GameModeClass->IsChildOf(
					ALobbyGameMode::StaticClass()));
	}
}
#endif

APdWorldSettings::APdWorldSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#if WITH_EDITOR
EDataValidationResult APdWorldSettings::IsDataValid(
	FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (Result == EDataValidationResult::NotValidated)
	{
		Result = EDataValidationResult::Valid;
	}

	const UClass* GameModeClass =
		PdWorldSettings::ResolveEffectiveGameModeClass(*this);
	if (PdWorldSettings::RequiresExperience(*this, GameModeClass)
		&& !DefaultExperienceId.IsValid())
	{
		Result = EDataValidationResult::Invalid;
		Context.AddError(NSLOCTEXT(
			"PdWorldSettings",
			"MissingRequiredExperience",
			"DefaultExperienceId is required when the map uses an Experience-enabled GameMode."));
	}

	return Result;
}
#endif
