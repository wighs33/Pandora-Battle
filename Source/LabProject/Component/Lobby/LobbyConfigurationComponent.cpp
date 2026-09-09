#include "Component/Lobby/LobbyConfigurationComponent.h"

#include "Common/GameSessionConstants.h"
#include "Definition/Provision/DefaultProvisionDefinition.h"
#include "Definition/Mode/PdGameInstanceDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Lobby/Contents/LobbyGameMode.h"
#include "Lobby/Contents/LobbyGameState.h"
#include "Lobby/Coordination/LobbyMatchCoordinator.h"
#include "Engine/GameInstance.h"
#include "Lobby/LobbyRuntimeSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LobbyConfigurationComponent)

DEFINE_LOG_CATEGORY_STATIC(LogLobbyConfiguration, Log, All);

ULobbyConfigurationComponent::ULobbyConfigurationComponent()
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
	RuntimeState = ERuntimeState::Loading;
	RuntimeReadyDelegate = MoveTemp(OnReady);
	const uint32 RequestGeneration = RuntimePreloadRequestGeneration;
	TArray<FSoftObjectPath> DependencyPaths;
	const FProjectDefinitionReferences& DefinitionReferences =
		UPdGameInstanceDefinition::GetConfiguredDefinitionReferences();
	if (!DefinitionReferences.MatchRule.IsNull())
	{
		DependencyPaths.AddUnique(
			DefinitionReferences.MatchRule.ToSoftObjectPath());
	}
	if (!DefinitionReferences.LevelDefinition.IsNull())
	{
		DependencyPaths.AddUnique(
			DefinitionReferences.LevelDefinition.ToSoftObjectPath());
	}
	if (!DefinitionReferences.DefaultProvision.IsNull())
	{
		DependencyPaths.AddUnique(
			DefinitionReferences.DefaultProvision.ToSoftObjectPath());
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
	if (!LobbyDependenciesPreloadHandle.IsValid())
	{
		FinishRuntimeInitialization(RequestGeneration);
	}
}

void ULobbyConfigurationComponent::ApplyDefaultLobbyConfigIfNeeded()
{
	ALobbyGameMode* GameMode = GetLobbyGameMode();
	ULobbyRuntimeSubsystem* LobbySubsystem = GameMode
		? UGameInstance::GetSubsystem<ULobbyRuntimeSubsystem>(GameMode->GetGameInstance())
		: nullptr;
	if (!GameMode || !LobbySubsystem)
	{
		return;
	}

	if (!LobbySubsystem->GetLobbySelectedMapKey().IsNone())
	{
		const FName ResolvedMapKey = ResolveConfiguredMapKey(
			LobbySubsystem->GetLobbySelectedMapKey());
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
				LobbySubsystem->SetLobbyGameConfig(
					SelectedMapOption.MapKey,
					TravelMapName,
					SelectedMapOption.MaxPlayerCount,
					FMath::Clamp(
						LobbySubsystem->GetLobbyMaxBotCount(),
						0,
						100));

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
	LobbySubsystem->SetLobbyGameConfig(
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
	const int32 MaxBotCount =
		GetConfiguredMaxBotCount(
			SelectedMapOption.MapKey);
	if (ULobbyRuntimeSubsystem* LobbySubsystem =
		UGameInstance::GetSubsystem<ULobbyRuntimeSubsystem>(GameMode->GetGameInstance()))
	{
		LobbySubsystem->SetLobbyGameConfig(
			SelectedMapOption.MapKey,
			ResolveTravelMapName(
				SelectedMapOption.MapKey),
			SelectedMapOption.MaxPlayerCount,
			MaxBotCount);
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
	ULobbyRuntimeSubsystem* LobbySubsystem = GameMode
		? UGameInstance::GetSubsystem<ULobbyRuntimeSubsystem>(GameMode->GetGameInstance())
		: nullptr;
	if (!GameMode || !LobbySubsystem)
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
	LobbySubsystem->SetLobbyGameConfig(
		ResolvedMapKey,
		TravelMapName,
		MaxPlayerCount,
		MaxBotCount);

	if (bHasSelectedMapOption)
	{
		SelectedMapOption.MaxPlayerCount =
			MaxPlayerCount;
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
}

FString ULobbyConfigurationComponent::GetRoomTravelMapName()
{
	const ULevelDefinition* Definition = GetLevelDefinition();
	if (!Definition)
	{
		return FString();
	}

	return Definition->GetRoomTravelMapName();
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
	if (const ULevelDefinition* Levels = GetLevelDefinition())
	{
		FLobbyMatchMapOption MapOption;
		if (Levels->GetIngameLevelAtIndex(
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
	const ULevelDefinition* Levels = GetLevelDefinition();
	return Levels
		? Levels->IngameLevels.Num()
		: 0;
}

bool ULobbyConfigurationComponent::GetLobbyMapOptionAtIndex(
	const int32 Index,
	FLobbyMatchMapOption& OutMapOption)
{
	const ULevelDefinition* Levels = GetLevelDefinition();
	return Levels
		&& Levels->GetIngameLevelAtIndex(
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
	const ULobbyRuntimeSubsystem* LobbySubsystem = GameMode
		? UGameInstance::GetSubsystem<ULobbyRuntimeSubsystem>(GameMode->GetGameInstance())
		: nullptr;
	if (LobbySubsystem
		&& !LobbySubsystem->GetLobbySelectedMapKey().IsNone())
	{
		return ResolveConfiguredMapKey(
			LobbySubsystem->GetLobbySelectedMapKey());
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
		&& MatchCoordinator->IsGameStartRequested())
	{
		MatchCoordinator->CancelPendingGameStart(
			TEXT("map_changed"));
	}

	SaveConfig(
		NewMapOption.MapKey,
		NewMapOption.MaxPlayerCount,
		GetConfiguredMaxBotCount(NewMapOption.MapKey));
}

FName ULobbyConfigurationComponent::ResolveConfiguredMapKey(
	const FName MapKey)
{
	const ULevelDefinition* Levels = GetLevelDefinition();
	return Levels
		? Levels->ResolveIngameLevelKey(MapKey)
		: MapKey;
}

bool ULobbyConfigurationComponent::FindConfiguredMapOption(
	const FName MapKey,
	FLobbyMatchMapOption& OutMapOption)
{
	const ULevelDefinition* Levels = GetLevelDefinition();
	return Levels
		&& Levels->FindIngameLevel(
			MapKey,
			OutMapOption);
}

int32 ULobbyConfigurationComponent::
GetConfiguredMaxPlayerCount()
{
	const ALobbyGameMode* GameMode = GetLobbyGameMode();
	const ULobbyRuntimeSubsystem* LobbySubsystem = GameMode
		? UGameInstance::GetSubsystem<ULobbyRuntimeSubsystem>(GameMode->GetGameInstance())
		: nullptr;
	const FName SelectedMapKey =
		LobbySubsystem
			&& !LobbySubsystem
				->GetLobbySelectedMapKey().IsNone()
			? LobbySubsystem->GetLobbySelectedMapKey()
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
		LobbySubsystem
			? LobbySubsystem->GetLobbyMaxPlayerCount()
			: LabGameSession::MaxPlayerCount,
		1);
}

int32 ULobbyConfigurationComponent::GetConfiguredMaxBotCount(
	const FName MapKey)
{
	static_cast<void>(MapKey);

	const ALobbyGameMode* GameMode = GetLobbyGameMode();
	const ULobbyRuntimeSubsystem* LobbySubsystem = GameMode
		? UGameInstance::GetSubsystem<ULobbyRuntimeSubsystem>(GameMode->GetGameInstance())
		: nullptr;
	return FMath::Clamp(
		LobbySubsystem
			? LobbySubsystem->GetLobbyMaxBotCount()
			: 10,
		0,
		100);
}

const ULevelDefinition*
ULobbyConfigurationComponent::GetLevelDefinition()
{
	if (LoadedLevelDefinition)
	{
		return LoadedLevelDefinition;
	}

	LoadedLevelDefinition =
		UPdGameInstanceDefinition::GetConfiguredDefinitionReferences()
			.LevelDefinition.Get();
	if (!LoadedLevelDefinition)
	{
		if (!bLoggedMissingLevelDefinition)
		{
			bLoggedMissingLevelDefinition = true;
			UE_LOG(
				LogLobbyConfiguration,
				Error,
				TEXT("Required LevelDefinition is missing; using native defaults."));
		}
		LoadedLevelDefinition = GetMutableDefault<ULevelDefinition>();
	}

	return LoadedLevelDefinition;
}

const UMatchRuleDefinition*
ULobbyConfigurationComponent::GetMatchRuleDefinition()
{
	if (LoadedMatchRuleDefinition)
	{
		return LoadedMatchRuleDefinition;
	}

	LoadedMatchRuleDefinition =
		UPdGameInstanceDefinition::GetConfiguredDefinitionReferences()
			.MatchRule.Get();
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

const UDefaultProvisionDefinition*
ULobbyConfigurationComponent::GetDefaultProvisionDefinition() const
{
	return IsRuntimeReady() ? LoadedDefaultProvisionDefinition.Get() : nullptr;
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

void ULobbyConfigurationComponent::HandleLobbyDependenciesPreloadComplete(
	const uint32 RequestGeneration)
{
	FinishRuntimeInitialization(RequestGeneration);
}

void ULobbyConfigurationComponent::FinishRuntimeInitialization(
	const uint32 RequestGeneration)
{
	if (RequestGeneration != RuntimePreloadRequestGeneration || RuntimeState != ERuntimeState::Loading)
	{
		return;
	}

	const FProjectDefinitionReferences& DefinitionReferences =
		UPdGameInstanceDefinition::GetConfiguredDefinitionReferences();
	LoadedLevelDefinition = DefinitionReferences.LevelDefinition.Get();
	LoadedMatchRuleDefinition = DefinitionReferences.MatchRule.Get();
	LoadedDefaultProvisionDefinition = DefinitionReferences.DefaultProvision.Get();
	if (!LoadedDefaultProvisionDefinition)
	{
		RuntimeState = ERuntimeState::Failed;
		RuntimeReadyDelegate.Unbind();
		UE_LOG(LogLobbyConfiguration, Error,
			TEXT("Required DA_DefaultProvision failed to load: %s. Lobby player start is blocked."),
			*DefinitionReferences.DefaultProvision.ToSoftObjectPath().ToString());
		return;
	}
	if (!LoadedLevelDefinition)
	{
		LoadedLevelDefinition = GetMutableDefault<ULevelDefinition>();
	}
	if (!LoadedMatchRuleDefinition)
	{
		LoadedMatchRuleDefinition = GetMutableDefault<UMatchRuleDefinition>();
	}

	RuntimeState = ERuntimeState::Ready;
	FSimpleDelegate ReadyDelegate = MoveTemp(RuntimeReadyDelegate);
	RuntimeReadyDelegate.Unbind();
	ReadyDelegate.ExecuteIfBound();
}

void ULobbyConfigurationComponent::ReleaseRuntimePreloads()
{
	RuntimeState = ERuntimeState::NotStarted;
	LoadedDefaultProvisionDefinition = nullptr;
	++RuntimePreloadRequestGeneration;
	RuntimeReadyDelegate.Unbind();
	if (LobbyDependenciesPreloadHandle.IsValid())
	{
		LobbyDependenciesPreloadHandle->CancelHandle();
		LobbyDependenciesPreloadHandle->ReleaseHandle();
		LobbyDependenciesPreloadHandle.Reset();
	}
}
