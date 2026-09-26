#include "Component/Lobby/LobbyConfigurationComponent.h"

#include "Definition/Provision/DefaultProvisionDefinition.h"
#include "Definition/Mode/PdGameInstanceDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Lobby/Contents/LobbyGameMode.h"
#include "Lobby/Contents/LobbyGameState.h"
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
				&ThisClass::FinishRuntimeInitialization,
				RequestGeneration));
	if (!LobbyDependenciesPreloadHandle.IsValid())
	{
		FinishRuntimeInitialization(RequestGeneration);
	}
}

void ULobbyConfigurationComponent::ApplyDefaultLobbyConfigIfNeeded()
{
	ULobbyRuntimeSubsystem* Runtime = GetWorld()->GetGameInstance()->GetSubsystem<ULobbyRuntimeSubsystem>();
	const FName Key = Runtime->GetLobbySelectedMapKey();
	SaveConfig(Key.IsNone() ? GetFirstMapKey() : Key, Runtime->GetLobbyMaxBotCount());
}

void ULobbyConfigurationComponent::SaveConfig(FName MapKey, int32 InMaxBotCount)
{
	ALobbyGameMode* GameMode = GetLobbyGameMode();
	ULobbyRuntimeSubsystem* Runtime = GameMode->GetGameInstance()->GetSubsystem<ULobbyRuntimeSubsystem>();
	FLobbyMatchMapOption Option;
	if (!FindConfiguredMapOption(MapKey, Option)) { return; }
	Runtime->SetLobbyGameConfig(Option.MapKey, Option.Map.ToSoftObjectPath().GetLongPackageName(), Option.MaxPlayerCount, FMath::Clamp(InMaxBotCount, 0, 100));
	if (ALobbyGameState* State = GameMode->GetGameState<ALobbyGameState>()) { State->SetSelectedMapOption(Option); }
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
	const ALobbyGameState* State = GetLobbyGameMode()->GetGameState<ALobbyGameState>();
	return State ? State->GetSelectedMapKey() : NAME_None;
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

int32 ULobbyConfigurationComponent::GetConfiguredMaxPlayerCount()
{
	FLobbyMatchMapOption Option;
	return GetSelectedLobbyMapOption(Option) ? Option.MaxPlayerCount : 0;
}

int32 ULobbyConfigurationComponent::GetConfiguredMaxBotCount()
{
	return GetWorld()->GetGameInstance()->GetSubsystem<ULobbyRuntimeSubsystem>()->GetLobbyMaxBotCount();
}

const ULevelDefinition* ULobbyConfigurationComponent::GetLevelDefinition()
{
	return LoadedLevelDefinition;
}

const UMatchRuleDefinition* ULobbyConfigurationComponent::GetMatchRuleDefinition()
{
	return LoadedMatchRuleDefinition;
}

const UDefaultProvisionDefinition*
ULobbyConfigurationComponent::GetDefaultProvisionDefinition() const
{
	return LoadedDefaultProvisionDefinition;
}

ALobbyGameMode*
ULobbyConfigurationComponent::GetLobbyGameMode() const
{
	return Cast<ALobbyGameMode>(GetOwner());
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
	ULobbyRuntimeSubsystem* Runtime = GetWorld()->GetGameInstance()->GetSubsystem<ULobbyRuntimeSubsystem>();
	FLobbyMatchMapOption Option;
	const FName Key = Runtime ? Runtime->GetLobbySelectedMapKey() : NAME_None;
	const bool bHasMap = LoadedLevelDefinition && (Key.IsNone()
		? LoadedLevelDefinition->GetIngameLevelAtIndex(0, Option)
		: LoadedLevelDefinition->FindIngameLevel(Key, Option));
	if (!LoadedLevelDefinition || !LoadedMatchRuleDefinition || !LoadedDefaultProvisionDefinition || !Runtime
		|| !bHasMap)
	{
		RuntimeState = ERuntimeState::Failed;
		RuntimeReadyDelegate.Unbind();
		UE_LOG(LogLobbyConfiguration, Error, TEXT("Required lobby definitions or selected map failed to initialize. Player start is blocked."));
		return;
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
	LoadedLevelDefinition = nullptr;
	LoadedMatchRuleDefinition = nullptr;
	++RuntimePreloadRequestGeneration;
	RuntimeReadyDelegate.Unbind();
	if (LobbyDependenciesPreloadHandle.IsValid())
	{
		LobbyDependenciesPreloadHandle->CancelHandle();
		LobbyDependenciesPreloadHandle->ReleaseHandle();
		LobbyDependenciesPreloadHandle.Reset();
	}
}
