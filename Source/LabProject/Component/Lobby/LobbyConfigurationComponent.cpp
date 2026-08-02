#include "Component/Lobby/LobbyConfigurationComponent.h"

#include "Common/GameSessionConstants.h"
#include "Definition/Lobby/LobbyModeDefinition.h"
#include "Definition/Lobby/LobbyPreviewDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Lobby/Contents/LobbyGameMode.h"
#include "Lobby/Contents/LobbyGameState.h"
#include "Lobby/Coordination/LobbyMatchCoordinator.h"
#include "Mode/PdGameInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LobbyConfigurationComponent)

DEFINE_LOG_CATEGORY_STATIC(LogLobbyConfiguration, Log, All);

ULobbyConfigurationComponent::ULobbyConfigurationComponent()
	: LobbyModeDefinition(
		ULobbyModeDefinition::GetDefaultDefinitionPath())
{
	PrimaryComponentTick.bCanEverTick = false;
}

void ULobbyConfigurationComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	ReleaseRuntimePreloads();
	Super::EndPlay(EndPlayReason);
}

void ULobbyConfigurationComponent::InitializeRuntime(FSimpleDelegate OnReady)
{
	ReleaseRuntimePreloads();
	RuntimeReadyDelegate = MoveTemp(OnReady);
	LoadedLobbyModeDefinition = LobbyModeDefinition.Get();
	const uint32 RequestGeneration = RuntimePreloadRequestGeneration;
	if (LoadedLobbyModeDefinition || LobbyModeDefinition.IsNull())
	{
		HandleLobbyModePreloadComplete(RequestGeneration);
		return;
	}

	LobbyModePreloadHandle =
		UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
			LobbyModeDefinition.ToSoftObjectPath(),
			FStreamableDelegate::CreateUObject(
				this,
				&ThisClass::HandleLobbyModePreloadComplete,
				RequestGeneration));
	if (!LobbyModePreloadHandle.IsValid())
	{
		HandleLobbyModePreloadComplete(RequestGeneration);
	}
}

void ULobbyConfigurationComponent::ApplyDefaultLobbyConfigIfNeeded()
{
	ALobbyGameMode* GameMode = GetLobbyGameMode();
	UPdGameInstance* GameInstance = GameMode
		? GameMode->GetGameInstance<UPdGameInstance>()
		: nullptr;
	if (!GameMode || !GameInstance)
	{
		return;
	}

	if (!GameInstance->GetLobbySelectedMapKey().IsNone())
	{
		const FName ResolvedMapKey = ResolveConfiguredMapKey(
			GameInstance->GetLobbySelectedMapKey());
		FLobbyMatchMapOption SelectedMapOption;
		if (FindConfiguredMapOption(
			ResolvedMapKey,
			SelectedMapOption))
		{
			const FString TravelMapName =
				ResolveTravelMapName(
					SelectedMapOption.MapKey);
			if (!TravelMapName.IsEmpty())
			{
				SelectedMapOption.MaxPlayerCount =
					FMath::Max(
						SelectedMapOption.MaxPlayerCount,
						1);
				SelectedMapOption.DefaultMaxBotCount =
					FMath::Clamp(
						SelectedMapOption
							.DefaultMaxBotCount,
						0,
						100);
				GameInstance->SetLobbyGameConfig(
					SelectedMapOption.MapKey,
					TravelMapName,
					SelectedMapOption.MaxPlayerCount,
					SelectedMapOption
						.DefaultMaxBotCount);

				if (ALobbyGameState* LobbyGameState =
					GameMode->GetGameState<
						ALobbyGameState>())
				{
					LobbyGameState->SetSelectedMapOption(
						SelectedMapOption);
				}
				return;
			}
		}
	}

	const FName FirstMapKey = GetFirstMapKey();
	const FString FirstTravelMapName =
		ResolveTravelMapName(FirstMapKey);
	const int32 FirstMaxPlayerCount =
		GetConfiguredMaxPlayerCount();
	const int32 FirstMaxBotCount =
		GetConfiguredMaxBotCount(FirstMapKey);
	GameInstance->SetLobbyGameConfig(
		FirstMapKey,
		FirstTravelMapName,
		FirstMaxPlayerCount,
		FirstMaxBotCount);

	FLobbyMatchMapOption FirstMapOption;
	if (FindConfiguredMapOption(
		FirstMapKey,
		FirstMapOption))
	{
		FirstMapOption.MaxPlayerCount =
			FirstMaxPlayerCount;
		FirstMapOption.DefaultMaxBotCount =
			FirstMaxBotCount;
		if (ALobbyGameState* LobbyGameState =
			GameMode->GetGameState<ALobbyGameState>())
		{
			LobbyGameState->SetSelectedMapOption(
				FirstMapOption);
		}
	}
}

void ULobbyConfigurationComponent::
SyncSelectedLobbyConfigToRuntime()
{
	ALobbyGameMode* GameMode = GetLobbyGameMode();
	if (!GameMode)
	{
		return;
	}

	FLobbyMatchMapOption SelectedMapOption;
	if (!FindConfiguredMapOption(
		GetSelectedLobbyMapKey(),
		SelectedMapOption))
	{
		return;
	}

	SelectedMapOption.MaxPlayerCount =
		GetConfiguredMaxPlayerCount();
	SelectedMapOption.DefaultMaxBotCount =
		GetConfiguredMaxBotCount(
			SelectedMapOption.MapKey);
	if (UPdGameInstance* GameInstance =
		GameMode->GetGameInstance<UPdGameInstance>())
	{
		GameInstance->SetLobbyGameConfig(
			SelectedMapOption.MapKey,
			ResolveTravelMapName(
				SelectedMapOption.MapKey),
			SelectedMapOption.MaxPlayerCount,
			SelectedMapOption.DefaultMaxBotCount);
	}

	if (ALobbyGameState* LobbyGameState =
		GameMode->GetGameState<ALobbyGameState>())
	{
		LobbyGameState->SetSelectedMapOption(
			SelectedMapOption);
	}
}

void ULobbyConfigurationComponent::SaveConfig(
	const FName MapKey,
	const int32 InMaxPlayerCount,
	const int32 InMaxBotCount)
{
	static_cast<void>(InMaxPlayerCount);

	ALobbyGameMode* GameMode = GetLobbyGameMode();
	UPdGameInstance* GameInstance = GameMode
		? GameMode->GetGameInstance<UPdGameInstance>()
		: nullptr;
	if (!GameMode || !GameInstance)
	{
		return;
	}

	const FName ResolvedMapKey =
		ResolveConfiguredMapKey(MapKey);
	const FString TravelMapName =
		ResolveTravelMapName(ResolvedMapKey);
	FLobbyMatchMapOption SelectedMapOption;
	const bool bHasSelectedMapOption =
		FindConfiguredMapOption(
			ResolvedMapKey,
			SelectedMapOption);
	const int32 MaxPlayerCount =
		bHasSelectedMapOption
			? FMath::Max(
				SelectedMapOption.MaxPlayerCount,
				1)
			: LabGameSession::MaxPlayerCount;

	ULobbyMatchCoordinator* MatchCoordinator =
		GameMode->GetMatchCoordinator();

	const int32 MaxBotCount = FMath::Clamp(
		InMaxBotCount,
		0,
		100);
	GameInstance->SetLobbyGameConfig(
		ResolvedMapKey,
		TravelMapName,
		MaxPlayerCount,
		MaxBotCount);

	if (bHasSelectedMapOption)
	{
		SelectedMapOption.MaxPlayerCount =
			MaxPlayerCount;
		SelectedMapOption.DefaultMaxBotCount =
			GameInstance->GetLobbyMaxBotCount();
		if (ALobbyGameState* LobbyGameState =
			GameMode->GetGameState<ALobbyGameState>())
		{
			LobbyGameState->SetSelectedMapOption(
				SelectedMapOption);
		}
	}

	if (MatchCoordinator)
	{
		MatchCoordinator
			->UpdateAdvertisedSessionSettings(
				ResolvedMapKey,
				MaxPlayerCount);
	}
	GameMode->RefreshLobbyUIForAllPlayers();
	if (MatchCoordinator)
	{
		MatchCoordinator
			->UpdateFullLobbyAutoStartTimer();
	}
}

FString ULobbyConfigurationComponent::GetRoomTravelMapName()
{
	const ULobbyModeDefinition* Definition =
		GetLobbyModeDefinition();
	if (!Definition)
	{
		return FString();
	}

	const FLobbyTravelSettings& Settings =
		Definition->GetTravelSettings();
	return ResolveSoftMapPath(
		Settings.RoomMap,
		Settings.RoomTravelMapName);
}

FString ULobbyConfigurationComponent::ResolveTravelMapName(
	const FName MapKey)
{
	FLobbyMatchMapOption MapOption;
	return FindConfiguredMapOption(MapKey, MapOption)
		? ResolveSoftMapPath(
			MapOption.Map,
			MapOption.TravelMapName)
		: FString();
}

FName ULobbyConfigurationComponent::GetFirstMapKey()
{
	if (const UMatchRuleDefinition* MatchRules =
		GetMatchRuleDefinition())
	{
		FLobbyMatchMapOption MapOption;
		if (MatchRules->GetLobbyMapOptionAtIndex(
			0,
			MapOption))
		{
			return MapOption.MapKey;
		}
	}
	return NAME_None;
}

int32 ULobbyConfigurationComponent::GetLobbyMapOptionCount()
{
	const UMatchRuleDefinition* MatchRules =
		GetMatchRuleDefinition();
	return MatchRules
		? MatchRules->LobbyMapOptions.Num()
		: 0;
}

bool ULobbyConfigurationComponent::GetLobbyMapOptionAtIndex(
	const int32 Index,
	FLobbyMatchMapOption& OutMapOption)
{
	const UMatchRuleDefinition* MatchRules =
		GetMatchRuleDefinition();
	return MatchRules
		&& MatchRules->GetLobbyMapOptionAtIndex(
			Index,
			OutMapOption);
}

bool ULobbyConfigurationComponent::
GetSelectedLobbyMapOption(
	FLobbyMatchMapOption& OutMapOption)
{
	return FindConfiguredMapOption(
		GetSelectedLobbyMapKey(),
		OutMapOption);
}

FName ULobbyConfigurationComponent::GetSelectedLobbyMapKey()
{
	const ALobbyGameMode* GameMode = GetLobbyGameMode();
	const UPdGameInstance* GameInstance = GameMode
		? GameMode->GetGameInstance<UPdGameInstance>()
		: nullptr;
	if (GameInstance
		&& !GameInstance->GetLobbySelectedMapKey().IsNone())
	{
		return ResolveConfiguredMapKey(
			GameInstance->GetLobbySelectedMapKey());
	}

	return GetFirstMapKey();
}

int32 ULobbyConfigurationComponent::
GetSelectedLobbyMaxPlayerCount()
{
	return GetConfiguredMaxPlayerCount();
}

void ULobbyConfigurationComponent::SelectLobbyMapByOffset(
	const int32 Offset)
{
	ALobbyGameMode* GameMode = GetLobbyGameMode();
	const int32 OptionCount = GetLobbyMapOptionCount();
	if (!GameMode
		|| !GameMode->HasAuthority()
		|| Offset == 0
		|| OptionCount <= 0)
	{
		return;
	}

	const FName CurrentMapKey =
		GetSelectedLobbyMapKey();
	int32 CurrentIndex = 0;
	for (int32 Index = 0; Index < OptionCount; ++Index)
	{
		FLobbyMatchMapOption MapOption;
		if (GetLobbyMapOptionAtIndex(
			Index,
			MapOption)
			&& MapOption.MapKey == CurrentMapKey)
		{
			CurrentIndex = Index;
			break;
		}
	}

	const int32 WrappedIndex =
		(CurrentIndex + Offset % OptionCount + OptionCount)
		% OptionCount;
	FLobbyMatchMapOption NewMapOption;
	if (!GetLobbyMapOptionAtIndex(
		WrappedIndex,
		NewMapOption))
	{
		return;
	}

	ULobbyMatchCoordinator* MatchCoordinator =
		GameMode->GetMatchCoordinator();
	if (MatchCoordinator
		&& (MatchCoordinator->IsGameStartRequested()
			|| MatchCoordinator
				->IsFullLobbyAutoStartTimerActive()))
	{
		MatchCoordinator->CancelPendingGameStart(
			TEXT("map_changed"));
	}

	SaveConfig(
		NewMapOption.MapKey,
		NewMapOption.MaxPlayerCount,
		NewMapOption.DefaultMaxBotCount);
}

FName ULobbyConfigurationComponent::ResolveConfiguredMapKey(
	const FName MapKey)
{
	const UMatchRuleDefinition* MatchRules =
		GetMatchRuleDefinition();
	return MatchRules
		? MatchRules->ResolveLobbyMapKey(MapKey)
		: MapKey;
}

bool ULobbyConfigurationComponent::FindConfiguredMapOption(
	const FName MapKey,
	FLobbyMatchMapOption& OutMapOption)
{
	const UMatchRuleDefinition* MatchRules =
		GetMatchRuleDefinition();
	return MatchRules
		&& MatchRules->FindLobbyMapOption(
			MapKey,
			OutMapOption);
}

int32 ULobbyConfigurationComponent::
GetConfiguredMaxPlayerCount()
{
	const ALobbyGameMode* GameMode = GetLobbyGameMode();
	const UPdGameInstance* GameInstance = GameMode
		? GameMode->GetGameInstance<UPdGameInstance>()
		: nullptr;
	const FName SelectedMapKey =
		GameInstance
			&& !GameInstance
				->GetLobbySelectedMapKey().IsNone()
			? GameInstance->GetLobbySelectedMapKey()
			: GetFirstMapKey();

	FLobbyMatchMapOption MapOption;
	if (FindConfiguredMapOption(
		SelectedMapKey,
		MapOption))
	{
		return FMath::Max(
			MapOption.MaxPlayerCount,
			1);
	}

	return FMath::Max(
		GameInstance
			? GameInstance->GetLobbyMaxPlayerCount()
			: LabGameSession::MaxPlayerCount,
		1);
}

int32 ULobbyConfigurationComponent::GetConfiguredMaxBotCount(
	const FName MapKey)
{
	FLobbyMatchMapOption MapOption;
	if (FindConfiguredMapOption(MapKey, MapOption))
	{
		return FMath::Clamp(
			MapOption.DefaultMaxBotCount,
			0,
			100);
	}

	const ALobbyGameMode* GameMode = GetLobbyGameMode();
	const UPdGameInstance* GameInstance = GameMode
		? GameMode->GetGameInstance<UPdGameInstance>()
		: nullptr;
	return FMath::Clamp(
		GameInstance
			? GameInstance->GetLobbyMaxBotCount()
			: 10,
		0,
		100);
}

const ULobbyModeDefinition*
ULobbyConfigurationComponent::GetLobbyModeDefinition()
{
	if (LoadedLobbyModeDefinition)
	{
		return LoadedLobbyModeDefinition;
	}

	if (!LobbyModeDefinition.IsNull())
	{
		LoadedLobbyModeDefinition =
			LobbyModeDefinition.Get();
	}
	if (!LoadedLobbyModeDefinition)
	{
		if (!bLoggedMissingLobbyModeDefinition)
		{
			bLoggedMissingLobbyModeDefinition = true;
			UE_LOG(
				LogLobbyConfiguration,
				Error,
				TEXT("Required LobbyModeDefinition could not be loaded from '%s'; using native defaults."),
				*LobbyModeDefinition.ToString());
		}
		LoadedLobbyModeDefinition =
			GetMutableDefault<ULobbyModeDefinition>();
	}

	return LoadedLobbyModeDefinition;
}

const UMatchRuleDefinition*
ULobbyConfigurationComponent::GetMatchRuleDefinition()
{
	if (LoadedMatchRuleDefinition)
	{
		return LoadedMatchRuleDefinition;
	}

	const ULobbyModeDefinition* Definition =
		GetLobbyModeDefinition();
	if (Definition)
	{
		LoadedMatchRuleDefinition =
			Definition->GetContentSettings()
				.MatchRuleDefinition.Get();
	}
	if (!LoadedMatchRuleDefinition)
	{
		if (!bLoggedMissingMatchRuleDefinition)
		{
			bLoggedMissingMatchRuleDefinition = true;
			UE_LOG(
				LogLobbyConfiguration,
				Error,
				TEXT("Required lobby MatchRuleDefinition is missing; using native defaults."));
		}
		LoadedMatchRuleDefinition =
			GetMutableDefault<UMatchRuleDefinition>();
	}

	return LoadedMatchRuleDefinition;
}

const ULobbyPreviewDefinition*
ULobbyConfigurationComponent::GetLobbyPreviewDefinition()
{
	if (LoadedLobbyPreviewDefinition)
	{
		return LoadedLobbyPreviewDefinition;
	}

	const ULobbyModeDefinition* Definition =
		GetLobbyModeDefinition();
	if (Definition)
	{
		LoadedLobbyPreviewDefinition =
			Definition->GetContentSettings()
				.LobbyPreviewDefinition.Get();
	}
	if (!LoadedLobbyPreviewDefinition)
	{
		if (!bLoggedMissingLobbyPreviewDefinition)
		{
			bLoggedMissingLobbyPreviewDefinition = true;
			UE_LOG(
				LogLobbyConfiguration,
				Error,
				TEXT("Required LobbyPreviewDefinition is missing; using native defaults."));
		}
		LoadedLobbyPreviewDefinition =
			GetMutableDefault<ULobbyPreviewDefinition>();
	}

	return LoadedLobbyPreviewDefinition;
}

ALobbyGameMode*
ULobbyConfigurationComponent::GetLobbyGameMode() const
{
	return Cast<ALobbyGameMode>(GetOwner());
}

FString ULobbyConfigurationComponent::ResolveSoftMapPath(
	const TSoftObjectPtr<UWorld>& Map,
	const FString& FallbackTravelMapName) const
{
	const FString LongPackageName =
		Map.ToSoftObjectPath().GetLongPackageName();
	return LongPackageName.IsEmpty()
		? FallbackTravelMapName
		: LongPackageName;
}

void ULobbyConfigurationComponent::HandleLobbyModePreloadComplete(
	const uint32 RequestGeneration)
{
	if (RequestGeneration != RuntimePreloadRequestGeneration)
	{
		return;
	}

	LoadedLobbyModeDefinition = LobbyModeDefinition.Get();
	if (!LoadedLobbyModeDefinition)
	{
		LoadedLobbyModeDefinition = GetMutableDefault<ULobbyModeDefinition>();
	}

	TArray<FSoftObjectPath> DependencyPaths;
	const FLobbyContentSettings& Content =
		LoadedLobbyModeDefinition->GetContentSettings();
	if (!Content.MatchRuleDefinition.IsNull())
	{
		DependencyPaths.AddUnique(
			Content.MatchRuleDefinition.ToSoftObjectPath());
	}
	if (!Content.LobbyPreviewDefinition.IsNull())
	{
		DependencyPaths.AddUnique(
			Content.LobbyPreviewDefinition.ToSoftObjectPath());
	}

	if (DependencyPaths.IsEmpty())
	{
		FinishRuntimeInitialization(RequestGeneration);
		return;
	}

	LobbyDependenciesPreloadHandle =
		UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
			DependencyPaths,
			FStreamableDelegate::CreateUObject(
				this,
				&ThisClass::HandleLobbyDependenciesPreloadComplete,
				RequestGeneration));
	if (LobbyModePreloadHandle.IsValid())
	{
		LobbyModePreloadHandle->ReleaseHandle();
		LobbyModePreloadHandle.Reset();
	}
	if (!LobbyDependenciesPreloadHandle.IsValid())
	{
		FinishRuntimeInitialization(RequestGeneration);
	}
}

void ULobbyConfigurationComponent::HandleLobbyDependenciesPreloadComplete(
	const uint32 RequestGeneration)
{
	FinishRuntimeInitialization(RequestGeneration);
}

void ULobbyConfigurationComponent::FinishRuntimeInitialization(
	const uint32 RequestGeneration)
{
	if (RequestGeneration != RuntimePreloadRequestGeneration)
	{
		return;
	}

	const FLobbyContentSettings& Content =
		LoadedLobbyModeDefinition->GetContentSettings();
	LoadedMatchRuleDefinition = Content.MatchRuleDefinition.Get();
	LoadedLobbyPreviewDefinition = Content.LobbyPreviewDefinition.Get();
	if (!LoadedMatchRuleDefinition)
	{
		LoadedMatchRuleDefinition = GetMutableDefault<UMatchRuleDefinition>();
	}
	if (!LoadedLobbyPreviewDefinition)
	{
		LoadedLobbyPreviewDefinition = GetMutableDefault<ULobbyPreviewDefinition>();
	}

	FSimpleDelegate ReadyDelegate = MoveTemp(RuntimeReadyDelegate);
	RuntimeReadyDelegate.Unbind();
	ReadyDelegate.ExecuteIfBound();
}

void ULobbyConfigurationComponent::ReleaseRuntimePreloads()
{
	++RuntimePreloadRequestGeneration;
	RuntimeReadyDelegate.Unbind();
	if (LobbyModePreloadHandle.IsValid())
	{
		LobbyModePreloadHandle->CancelHandle();
		LobbyModePreloadHandle->ReleaseHandle();
		LobbyModePreloadHandle.Reset();
	}
	if (LobbyDependenciesPreloadHandle.IsValid())
	{
		LobbyDependenciesPreloadHandle->CancelHandle();
		LobbyDependenciesPreloadHandle->ReleaseHandle();
		LobbyDependenciesPreloadHandle.Reset();
	}
}
