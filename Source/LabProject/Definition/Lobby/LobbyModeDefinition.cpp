#include "Definition/Lobby/LobbyModeDefinition.h"

#include "Definition/Mode/PdGameInstanceDefinition.h"
#include "Misc/PackageName.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LobbyModeDefinition)

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
}

FPrimaryAssetId ULobbyModeDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("LobbyModeDefinition"), GetFName());
}

FSoftObjectPath ULobbyModeDefinition::GetDefaultDefinitionPath()
{
	return UPdGameInstanceDefinition::GetConfiguredDefinitionReferences()
		.LobbyMode.ToSoftObjectPath();
}

const ULobbyModeDefinition* ULobbyModeDefinition::ResolveDefaultDefinition()
{
	const FSoftObjectPath DefinitionPath = GetDefaultDefinitionPath();
	if (!DefinitionPath.IsValid())
	{
		return nullptr;
	}

	if (const ULobbyModeDefinition* LoadedDefinition =
		Cast<ULobbyModeDefinition>(DefinitionPath.ResolveObject()))
	{
		return LoadedDefinition;
	}

	return Cast<ULobbyModeDefinition>(DefinitionPath.TryLoad());
}

FString ULobbyModeDefinition::GetTitleTravelMapName() const
{
	return ResolveMapPackageName(Travel.TitleMap);
}

FString ULobbyModeDefinition::GetLobbyTravelMapName() const
{
	return ResolveMapPackageName(Travel.LobbyMap);
}

FString ULobbyModeDefinition::GetRoomTravelMapName() const
{
	return ResolveMapPackageName(Travel.RoomMap);
}

bool ULobbyModeDefinition::IsLobbyMapName(const FString& LevelName) const
{
	return !LevelName.TrimStartAndEnd().IsEmpty()
		&& DoesMapMatchLevelName(Travel.LobbyMap, LevelName);
}

#if WITH_EDITOR
EDataValidationResult ULobbyModeDefinition::IsDataValid(
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
	if (Travel.TitleMap.IsNull())
	{
		MarkInvalid(NSLOCTEXT(
			"LobbyModeDefinition",
			"MissingTitleTravelDestination",
			"Travel.TitleMap is required."));
	}

	if (Travel.LobbyMap.IsNull())
	{
		MarkInvalid(NSLOCTEXT(
			"LobbyModeDefinition",
			"MissingLobbyTravelDestination",
			"Travel.LobbyMap is required."));
	}

	if (Travel.RoomMap.IsNull())
	{
		MarkInvalid(NSLOCTEXT(
			"LobbyModeDefinition",
			"MissingRoomTravelDestination",
			"Travel.RoomMap is required."));
	}

	return Result;
}
#endif
