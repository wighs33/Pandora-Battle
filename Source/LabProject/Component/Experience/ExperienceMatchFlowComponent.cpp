#include "Component/Experience/ExperienceMatchFlowComponent.h"

#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Common/GameSessionConstants.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Component/Experience/ExperiencePlayerProvisioningComponent.h"
#include "Component/Experience/ExperienceSpawnComponent.h"
#include "Component/Player/PlayerMatchComponent.h"
#include "Definition/Item/RewardDefinition.h"
#include "Definition/Level/LevelDefinition.h"
#include "Definition/Match/MatchRuleDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Item/RewardChest.h"
#include "Kismet/GameplayStatics.h"
#include "Map/ForceMoveGateActor.h"
#include "Misc/PackageName.h"
#include "Mode/ExperienceGameMode.h"
#include "Mode/ExperienceGameState.h"
#include "Mode/PdGameInstance.h"
#include "Mode/PdPlayerController.h"
#include "Mode/PdPlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ExperienceMatchFlowComponent)

DEFINE_LOG_CATEGORY_STATIC(LogExperienceMatchFlowContent, Log, All);

namespace
{
constexpr float GameResultLobbyReturnDelaySeconds = 5.0f;

FString StripTravelOptions(const FString& TravelMapName)
{
	FString CleanMapName = TravelMapName;
	int32 OptionsIndex = INDEX_NONE;
	if (CleanMapName.FindChar(TEXT('?'), OptionsIndex))
	{
		CleanMapName.LeftInline(OptionsIndex, EAllowShrinking::No);
	}
	return CleanMapName;
}

bool DoesMapOptionMatchWorld(
	const FLobbyMatchMapOption& MapOption,
	const FString& CurrentPackageName,
	const FString& CurrentLevelName)
{
	const FString MapPackageName =
		MapOption.Map.ToSoftObjectPath().GetLongPackageName();
	if (!MapPackageName.IsEmpty()
		&& (MapPackageName.Equals(
				CurrentPackageName,
				ESearchCase::IgnoreCase)
			|| FPackageName::GetShortName(MapPackageName).Equals(
				CurrentLevelName,
				ESearchCase::IgnoreCase)))
	{
		return true;
	}

	const FString TravelMapName =
		StripTravelOptions(MapOption.TravelMapName);
	if (!TravelMapName.IsEmpty()
		&& (TravelMapName.Equals(
				CurrentPackageName,
				ESearchCase::IgnoreCase)
			|| FPackageName::GetShortName(TravelMapName).Equals(
				CurrentLevelName,
				ESearchCase::IgnoreCase)
			|| TravelMapName.Equals(
				CurrentLevelName,
				ESearchCase::IgnoreCase)))
	{
		return true;
	}

	if (!MapOption.MapKey.IsNone())
	{
		const FString MapKeyString = MapOption.MapKey.ToString();
		return MapKeyString.Equals(
				CurrentLevelName,
				ESearchCase::IgnoreCase)
			|| MapKeyString.Equals(
				FPackageName::GetShortName(CurrentPackageName),
				ESearchCase::IgnoreCase);
	}

	return false;
}

bool IsEnabledTravelOption(
	const FString& Options,
	const TCHAR* OptionName)
{
	const FString OptionKey(OptionName);
	if (!UGameplayStatics::HasOption(Options, OptionKey))
	{
		return false;
	}

	FString OptionValue =
		UGameplayStatics::ParseOption(Options, OptionKey);
	OptionValue.TrimStartAndEndInline();
	return OptionValue.Equals(TEXT("1"), ESearchCase::IgnoreCase)
		|| OptionValue.Equals(TEXT("true"), ESearchCase::IgnoreCase)
		|| OptionValue.Equals(TEXT("yes"), ESearchCase::IgnoreCase);
}
}

UExperienceMatchFlowComponent::UExperienceMatchFlowComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UExperienceMatchFlowComponent::ApplySettings(
	const FExperienceMatchFlowSettings& InSettings)
{
	ReleaseRuntimeContentPreload();
	Settings = InSettings;
	BeginRuntimeContentPreload();
}

void UExperienceMatchFlowComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MatchTimerHandle);
		World->GetTimerManager().ClearTimer(ChestConfigurationRetryTimerHandle);
		World->GetTimerManager().ClearTimer(GameResultLobbyReturnTimerHandle);
	}
	MatchTimerHandle.Invalidate();
	ChestConfigurationRetryTimerHandle.Invalidate();
	GameResultLobbyReturnTimerHandle.Invalidate();
	ReleaseRuntimeContentPreload();

	Super::EndPlay(EndPlayReason);
}

AExperienceGameMode*
UExperienceMatchFlowComponent::GetExperienceGameMode() const
{
	return Cast<AExperienceGameMode>(GetOwner());
}

const AExperienceGameMode*
UExperienceMatchFlowComponent::GetExperienceGameModeConst() const
{
	return Cast<AExperienceGameMode>(GetOwner());
}

void UExperienceMatchFlowComponent::InitializeTravelOptions(
	const FString& Options)
{
	bMatchTimerSuppressedByTravelOption = IsEnabledTravelOption(
		Options,
		LabGameSession::NoMatchTimerOption);
}

void UExperienceMatchFlowComponent::InitializeGameState()
{
	if (bRuntimeContentLoadPending)
	{
		bInitializeGameStateRequested = true;
		return;
	}
	bInitializeGameStateRequested = false;

	AExperienceGameMode* GameMode = GetExperienceGameMode();
	AExperienceGameState* ExperienceGameState = GameMode
		? GameMode->GetGameState<AExperienceGameState>()
		: nullptr;
	if (!ExperienceGameState)
	{
		return;
	}

	ExperienceGameState->SetMatchRuleDefinition(
		const_cast<UMatchRuleDefinition*>(GetMatchRuleDefinition()));
	ExperienceGameState->SetMatchTimerState(
		ShouldSuppressServerMatchTimer()
			? EMatchTimerPhase::Suppressed
			: EMatchTimerPhase::Inactive);
}

void UExperienceMatchFlowComponent::StartServerMatchTimerIfNeeded()
{
	if (bRuntimeContentLoadPending)
	{
		bStartMatchTimerRequested = true;
		return;
	}
	bStartMatchTimerRequested = false;

	AExperienceGameMode* GameMode = GetExperienceGameMode();
	if (!GameMode || !GameMode->HasAuthority())
	{
		return;
	}

	AExperienceGameState* ExperienceGameState =
		GameMode->GetGameState<AExperienceGameState>();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MatchTimerHandle);
	}

	if (ShouldSuppressServerMatchTimer())
	{
		if (ExperienceGameState)
		{
			ExperienceGameState->SetMatchTimerState(
				EMatchTimerPhase::Suppressed);
		}
		return;
	}

	const UMatchRuleDefinition* MatchRules =
		GetMatchRuleDefinition();
	const float MatchTimerSeconds = MatchRules
		? MatchRules->MatchTimerSeconds
		: GetDefault<UMatchRuleDefinition>()->MatchTimerSeconds;
	if (MatchTimerSeconds <= 0.0f)
	{
		HandleMatchTimerExpired();
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (ExperienceGameState)
	{
		ExperienceGameState->SetMatchTimerState(
			EMatchTimerPhase::Running,
			ExperienceGameState->GetServerWorldTimeSeconds()
				+ MatchTimerSeconds);
	}
	World->GetTimerManager().SetTimer(
		MatchTimerHandle,
		this,
		&ThisClass::HandleMatchTimerExpired,
		MatchTimerSeconds,
		false);
}

void UExperienceMatchFlowComponent::ConfigureRewardChestSpawns()
{
	if (bRuntimeContentLoadPending)
	{
		bConfigureRewardChestsRequested = true;
		return;
	}
	bConfigureRewardChestsRequested = false;

	AExperienceGameMode* GameMode = GetExperienceGameMode();
	UWorld* World = GetWorld();
	if (!GameMode || !GameMode->HasAuthority() || !World)
	{
		return;
	}

	TArray<ARewardChest*> RewardChests;
	for (TActorIterator<ARewardChest> Iterator(World); Iterator; ++Iterator)
	{
		if (ARewardChest* RewardChest = *Iterator;
			IsValid(RewardChest))
		{
			RewardChests.Add(RewardChest);
		}
	}
	RewardChests.Sort(
		[](const ARewardChest& A, const ARewardChest& B)
		{
			return A.GetName() < B.GetName();
		});

	if (Settings.ChestSpawnRewardDefinition.IsNull())
	{
		for (const ARewardChest* RewardChest : RewardChests)
		{
			if (IsValid(RewardChest)
				&& !RewardChest->GetRewardDefinitionAsset().IsNull()
				&& !RewardChest->IsRewardContentReady())
			{
				ChestConfigurationRetryTimerHandle =
					World->GetTimerManager().SetTimerForNextTick(
						this,
						&ThisClass::ConfigureRewardChestSpawns);
				return;
			}
		}
	}
	World->GetTimerManager().ClearTimer(ChestConfigurationRetryTimerHandle);

	const URewardDefinition* RewardDefinition =
		ResolveRewardDefinitionForChestSpawns(RewardChests);
	if (RewardChests.IsEmpty() || !RewardDefinition)
	{
		return;
	}

	const int32 ActiveChestCount =
		RewardDefinition->ResolveActiveRewardChestCount(
			RewardChests.Num());
	if (ActiveChestCount >= RewardChests.Num())
	{
		return;
	}

	TArray<int32> ChestIndices;
	for (int32 Index = 0; Index < RewardChests.Num(); ++Index)
	{
		ChestIndices.Add(Index);
	}
	for (int32 Index = 0; Index < ActiveChestCount; ++Index)
	{
		const int32 SwapIndex =
			FMath::RandRange(Index, ChestIndices.Num() - 1);
		ChestIndices.Swap(Index, SwapIndex);
	}

	TSet<ARewardChest*> ActiveChests;
	for (int32 Index = 0; Index < ActiveChestCount; ++Index)
	{
		if (RewardChests.IsValidIndex(ChestIndices[Index]))
		{
			ActiveChests.Add(RewardChests[ChestIndices[Index]]);
		}
	}

	for (ARewardChest* RewardChest : RewardChests)
	{
		if (IsValid(RewardChest)
			&& !ActiveChests.Contains(RewardChest))
		{
			RewardChest->DeactivateForSpawnPool();
		}
	}
}

int32 UExperienceMatchFlowComponent::GrantGameVictoryGoldReward(
	AController* WinnerController,
	const int32 WinningTeamMemberCount)
{
	AExperienceGameMode* GameMode = GetExperienceGameMode();
	if (!GameMode || !GameMode->HasAuthority() || !WinnerController)
	{
		return 0;
	}

	UPdGameInstance* GameInstance =
		GameMode->GetGameInstance<UPdGameInstance>();
	if (!GameInstance)
	{
		return 0;
	}

	const APlayerController* WinnerPlayerController =
		Cast<APlayerController>(WinnerController);
	const FString PlayerId = GameInstance->ResolveSavePlayerId(
		WinnerPlayerController,
		WinnerController->PlayerState);
	const bool bWinnerLocal =
		WinnerPlayerController
		&& WinnerPlayerController->IsLocalController();
	const APdPlayerState* WinnerPlayerState =
		Cast<APdPlayerState>(WinnerController->PlayerState);
	const int32 GoldReward = CalculateVictoryGoldReward(
		WinnerPlayerState,
		WinningTeamMemberCount);
	int32 NewGold = 0;
	if (GoldReward > 0 && bWinnerLocal)
	{
		NewGold = GameInstance->AddGold(PlayerId, GoldReward, false);
		GameInstance->SetPreferredSavePlayerId(PlayerId);
		GameInstance->SaveGame(PlayerId);
	}

	if (APdPlayerController* WinnerPdPlayerController =
		Cast<APdPlayerController>(WinnerController);
		WinnerPdPlayerController && !bWinnerLocal && GoldReward > 0)
	{
		WinnerPdPlayerController->Client_AddGameVictoryGoldReward(
			PlayerId,
			GoldReward);
	}

	return NewGold;
}

void UExperienceMatchFlowComponent::HandleMatchTimerExpired()
{
	AExperienceGameMode* GameMode = GetExperienceGameMode();
	if (!GameMode
		|| !GameMode->HasAuthority()
		|| bMatchTimerExpired)
	{
		return;
	}

	bMatchTimerExpired = true;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MatchTimerHandle);
	}
	if (AExperienceGameState* ExperienceGameState =
		GameMode->GetGameState<AExperienceGameState>())
	{
		ExperienceGameState->SetMatchTimerState(
			EMatchTimerPhase::Expired);
	}

	APdPlayerState* WinnerPlayerState = nullptr;
	int32 TopKillCount = 0;
	bool bTopKillCountTied = false;
	if (TryFindUniqueKillLeader(
		WinnerPlayerState,
		TopKillCount,
		bTopKillCountTied))
	{
		ShowGameResultForWinner(WinnerPlayerState);
		return;
	}
	if (bTopKillCountTied
		&& TryFindSharedLeadingTeamWinner(
			WinnerPlayerState,
			TopKillCount))
	{
		// A player tie inside one team is already a team victory. Golden Kill is
		// only needed when the leading score is shared by opposing teams.
		ShowGameResultForWinner(WinnerPlayerState);
		return;
	}

	const UMatchRuleDefinition* MatchRules =
		GetMatchRuleDefinition();
	if (bTopKillCountTied
		&& MatchRules
		&& MatchRules->bGoldenKillEnabled)
	{
		StartGoldenKill(TopKillCount);
		ForceMovePlayersForGoldenKill();
	}
}

bool UExperienceMatchFlowComponent::ShowGameResultForWinner(
	APlayerState* WinnerPlayerState)
{
	AExperienceGameMode* GameMode = GetExperienceGameMode();
	AExperienceGameState* ExperienceGameState = GameMode
		? GameMode->GetGameState<AExperienceGameState>()
		: nullptr;
	if (!GameMode
		|| !GameMode->HasAuthority()
		|| !WinnerPlayerState
		|| bGameResultShown
		|| !ExperienceGameState)
	{
		return false;
	}

	bGameResultShown = true;
	bGoldenKillActive = false;
	GoldenKillVictoryScore = 0;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MatchTimerHandle);
	}
	ExperienceGameState->SetMatchTimerState(
		EMatchTimerPhase::Expired);

	APdPlayerState* TopKillerPlayerState = nullptr;
	int32 TopKillCount = 0;
	FindTopKiller(TopKillerPlayerState, TopKillCount);
	if (!TopKillerPlayerState)
	{
		TopKillerPlayerState = Cast<APdPlayerState>(WinnerPlayerState);
		TopKillCount = FMath::Max(
			FMath::RoundToInt(WinnerPlayerState->GetScore()),
			0);
	}

	const APdPlayerState* WinnerPdPlayerState =
		Cast<APdPlayerState>(WinnerPlayerState);
	const int32 WinnerTeamColorIndex = WinnerPdPlayerState
		? WinnerPdPlayerState->GetPlayerMatchComponent()
			->GetMatchTeamColorIndex()
		: INDEX_NONE;
	const int32 WinnerTeamMemberCount =
		CountPlayersOnTeam(WinnerTeamColorIndex);
	GrantVictoryRewardsForWinner(
		WinnerPlayerState,
		WinnerTeamColorIndex,
		WinnerTeamMemberCount);

	TArray<FGameResultPlayerStat> PlayerStats;
	BuildGameResultPlayerStats(PlayerStats);
	ApplyVictoryRewardEligibility(
		PlayerStats,
		WinnerPlayerState,
		WinnerTeamColorIndex,
		WinnerTeamMemberCount);

	ExperienceGameState->Multicast_ShowGameResult(
		ResolveWinnerTeamTitle(WinnerPlayerState),
		WinnerTeamColorIndex,
		ResolveResultPlayerName(
			TopKillerPlayerState
				? TopKillerPlayerState
				: WinnerPlayerState),
		TopKillCount,
		PlayerStats);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			GameResultLobbyReturnTimerHandle,
			this,
			&ThisClass::ReturnToLobbyAfterGameResult,
			GameResultLobbyReturnDelaySeconds,
			false);
	}
	return true;
}

void UExperienceMatchFlowComponent::NotifyPlayerKillScored(
	APlayerState* KillerPlayerState,
	APlayerState* VictimPlayerState)
{
	AExperienceGameMode* GameMode = GetExperienceGameMode();
	if (!GameMode
		|| !GameMode->HasAuthority()
		|| !bGoldenKillActive
		|| bGameResultShown
		|| !KillerPlayerState
		|| KillerPlayerState == VictimPlayerState)
	{
		return;
	}

	APdPlayerState* UniqueLeaderPlayerState = nullptr;
	int32 TopKillCount = 0;
	bool bTopKillCountTied = false;
	if (!TryFindUniqueKillLeader(
			UniqueLeaderPlayerState,
			TopKillCount,
			bTopKillCountTied)
		|| UniqueLeaderPlayerState != KillerPlayerState)
	{
		return;
	}

	if (ShowGameResultForWinner(UniqueLeaderPlayerState))
	{
		bGoldenKillActive = false;
		GoldenKillVictoryScore = 0;
	}
}

bool UExperienceMatchFlowComponent::RequestAbortMatchToTitle(
	APlayerController* RequestingPlayer)
{
	const AExperienceGameMode* GameMode =
		GetExperienceGameModeConst();
	if (!GameMode
		|| !GameMode->HasAuthority()
		|| !RequestingPlayer
		// Only the listen host may intentionally end the whole match. Remote
		// players leave locally and are handled by Logout on the server.
		|| !RequestingPlayer->IsLocalController()
		|| !AbortMatchToTitleForPlayerExit(
			RequestingPlayer->PlayerState))
	{
		return false;
	}

	const FString TitleMapName = GetResolvedTitleTravelMapName();
	if (APdPlayerController* PdPlayerController =
		Cast<APdPlayerController>(RequestingPlayer))
	{
		PdPlayerController->Client_TravelToTitleWithoutGameResult(
			TitleMapName);
	}
	else if (!TitleMapName.IsEmpty())
	{
		RequestingPlayer->ClientTravel(
			TitleMapName,
			TRAVEL_Absolute);
	}
	return true;
}

bool UExperienceMatchFlowComponent::HandlePlayerLogout(
	const APlayerState* ExitingPlayerState)
{
	const AExperienceGameMode* GameMode =
		GetExperienceGameModeConst();
	return GameMode
		&& GameMode->HasAuthority()
		&& ExitingPlayerState
		&& AbortMatchToTitleForPlayerExit(ExitingPlayerState);
}

bool UExperienceMatchFlowComponent::AbortMatchToTitleForPlayerExit(
	const APlayerState* ExitingPlayerState)
{
	AExperienceGameMode* GameMode = GetExperienceGameMode();
	if (!GameMode || !ShouldAbortMatchForPlayerExit(ExitingPlayerState))
	{
		return false;
	}

	bGameResultShown = true;
	bGoldenKillActive = false;
	GoldenKillVictoryScore = 0;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MatchTimerHandle);
	}
	if (AExperienceGameState* ExperienceGameState =
		GameMode->GetGameState<AExperienceGameState>())
	{
		ExperienceGameState->SetMatchTimerState(
			EMatchTimerPhase::Expired);
	}

	APdPlayerState* WinnerPlayerState = nullptr;
	int32 TopKillCount = 0;
	bool bTopKillCountTied = false;
	const bool bHasUniqueWinner = TryFindUniqueKillLeader(
		WinnerPlayerState,
		TopKillCount,
		bTopKillCountTied,
		ExitingPlayerState);
	if (!bHasUniqueWinner)
	{
		if (!bTopKillCountTied
			|| !TryFindSharedLeadingTeamWinner(
				WinnerPlayerState,
				TopKillCount,
				ExitingPlayerState))
		{
			// An opposing-team tie has no winner when the host aborts the match.
			WinnerPlayerState = nullptr;
		}
	}

	const int32 WinnerTeamColorIndex = WinnerPlayerState
		? WinnerPlayerState->GetPlayerMatchComponent()
			->GetMatchTeamColorIndex()
		: INDEX_NONE;
	const int32 WinnerTeamMemberCount =
		CountPlayersOnTeam(
			WinnerTeamColorIndex,
			ExitingPlayerState);
	if (WinnerPlayerState)
	{
		GrantVictoryRewardsForWinner(
			WinnerPlayerState,
			WinnerTeamColorIndex,
			WinnerTeamMemberCount,
			ExitingPlayerState);
	}

	SendPlayerExitGameResultToTitle(
		BuildPlayerExitGameResult(
			ExitingPlayerState,
			WinnerPlayerState,
			WinnerTeamColorIndex,
			WinnerTeamMemberCount),
		ExitingPlayerState);
	return true;
}

const UMatchRuleDefinition*
UExperienceMatchFlowComponent::GetMatchRuleDefinition() const
{
	if (!Settings.MatchRuleDefinition.IsNull())
	{
		if (const UMatchRuleDefinition* LoadedMatchRules =
			Settings.MatchRuleDefinition.Get())
		{
			return LoadedMatchRules;
		}
	}

	return GetDefault<UMatchRuleDefinition>();
}

const ULevelDefinition*
UExperienceMatchFlowComponent::GetLevelDefinition() const
{
	if (!Settings.LevelDefinition.IsNull())
	{
		if (const ULevelDefinition* LoadedLevels =
			Settings.LevelDefinition.Get())
		{
			return LoadedLevels;
		}
	}

	return GetDefault<ULevelDefinition>();
}

bool UExperienceMatchFlowComponent::FindCurrentMatchMapOption(
	FLobbyMatchMapOption& OutMapOption) const
{
	const ULevelDefinition* Levels = GetLevelDefinition();
	if (!Levels || Levels->IngameLevels.IsEmpty())
	{
		return false;
	}

	const UWorld* CurrentWorld = GetWorld();
	const FString CurrentPackageName =
		CurrentWorld && CurrentWorld->GetOutermost()
			? CurrentWorld->GetOutermost()->GetName()
			: FString();
	const FString CurrentLevelName =
		UGameplayStatics::GetCurrentLevelName(this, true);
	const bool bHasCurrentLevelContext =
		!CurrentPackageName.IsEmpty() || !CurrentLevelName.IsEmpty();

	const AExperienceGameMode* GameMode =
		GetExperienceGameModeConst();
	const UPdGameInstance* GameInstance = GameMode
		? GameMode->GetGameInstance<UPdGameInstance>()
		: nullptr;
	if (GameInstance)
	{
		const FName SelectedMapKey =
			GameInstance->GetLobbySelectedMapKey();
		if (!SelectedMapKey.IsNone()
			&& Levels->FindIngameLevel(
				SelectedMapKey,
				OutMapOption)
			&& (!bHasCurrentLevelContext
				|| DoesMapOptionMatchWorld(
					OutMapOption,
					CurrentPackageName,
					CurrentLevelName)))
		{
			return true;
		}
	}

	for (const FLobbyMatchMapOption& MapOption :
		Levels->IngameLevels)
	{
		if (DoesMapOptionMatchWorld(
			MapOption,
			CurrentPackageName,
			CurrentLevelName))
		{
			OutMapOption = MapOption;
			return true;
		}
	}

	return false;
}

int32 UExperienceMatchFlowComponent::CalculateVictoryGoldReward(
	const int32 KillCount,
	const int32 DeathCount,
	const int32 WinningTeamMemberCount) const
{
	const int32 RawReward =
		FMath::Max(KillCount, 0)
			* FMath::Max(Settings.VictoryGoldPerKill, 0)
		- FMath::Max(DeathCount, 0)
			* FMath::Max(
				Settings.VictoryGoldPenaltyPerDeath,
				0)
		+ FMath::Max(WinningTeamMemberCount, 1)
			* FMath::Max(
				Settings.VictoryGoldPerWinningTeamMember,
				0);
	return FMath::Max(RawReward, 0);
}

int32 UExperienceMatchFlowComponent::CalculateGoldenKillVictoryScore(
	const int32 TopKillCount)
{
	return FMath::Max(TopKillCount, 0) + 1;
}

bool UExperienceMatchFlowComponent::HasReachedGoldenKillVictoryScore(
	const int32 KillCount,
	const int32 VictoryScore)
{
	return VictoryScore > 0
		&& FMath::Max(KillCount, 0) >= VictoryScore;
}

bool UExperienceMatchFlowComponent::ShouldEnterGoldenKillForLeaderTeams(
	const TArray<int32>& LeaderTeamColorIndices)
{
	if (LeaderTeamColorIndices.Num() < 2)
	{
		return false;
	}

	const int32 FirstTeamColorIndex = LeaderTeamColorIndices[0];
	if (FirstTeamColorIndex == INDEX_NONE)
	{
		// Players without an assigned team are independent competitors.
		return true;
	}

	for (int32 Index = 1; Index < LeaderTeamColorIndices.Num(); ++Index)
	{
		if (LeaderTeamColorIndices[Index] == INDEX_NONE
			|| LeaderTeamColorIndices[Index] != FirstTeamColorIndex)
		{
			return true;
		}
	}

	return false;
}

bool UExperienceMatchFlowComponent::
ShouldSuppressServerMatchTimerForCurrentMap() const
{
	const UMatchRuleDefinition* MatchRules =
		GetMatchRuleDefinition();
	if (!MatchRules || MatchRules->MapsWithoutMatchTimer.IsEmpty())
	{
		return false;
	}

	const FString CurrentLevelName =
		UGameplayStatics::GetCurrentLevelName(this, true);
	return MatchRules->MapsWithoutMatchTimer.Contains(
		FName(*CurrentLevelName));
}

bool UExperienceMatchFlowComponent::ShouldSuppressServerMatchTimer() const
{
	return bMatchTimerSuppressedByTravelOption
		|| ShouldSuppressServerMatchTimerForCurrentMap();
}

bool UExperienceMatchFlowComponent::TryFindUniqueKillLeader(
	APdPlayerState*& OutWinnerPlayerState,
	int32& OutTopKillCount,
	bool& bOutTie,
	const APlayerState* ExcludedPlayerState) const
{
	OutWinnerPlayerState = nullptr;
	OutTopKillCount = 0;
	bOutTie = false;

	const AExperienceGameMode* GameMode =
		GetExperienceGameModeConst();
	const AGameStateBase* CurrentGameState =
		GameMode
			? GameMode->GetGameState<AGameStateBase>()
			: nullptr;
	if (!CurrentGameState)
	{
		return false;
	}

	bool bHasAnyPlayer = false;
	int32 TopKillCount = MIN_int32;
	for (APlayerState* PlayerState : CurrentGameState->PlayerArray)
	{
		if (PlayerState == ExcludedPlayerState)
		{
			continue;
		}

		APdPlayerState* PdPlayerState =
			Cast<APdPlayerState>(PlayerState);
		if (!PdPlayerState)
		{
			continue;
		}

		bHasAnyPlayer = true;
		const int32 KillCount = FMath::Max(
			FMath::RoundToInt(PdPlayerState->GetScore()),
			0);
		if (KillCount > TopKillCount)
		{
			TopKillCount = KillCount;
			OutWinnerPlayerState = PdPlayerState;
			bOutTie = false;
		}
		else if (KillCount == TopKillCount)
		{
			bOutTie = true;
		}
	}

	if (!bHasAnyPlayer || !OutWinnerPlayerState)
	{
		return false;
	}

	OutTopKillCount = FMath::Max(TopKillCount, 0);
	return !bOutTie;
}

bool UExperienceMatchFlowComponent::TryFindSharedLeadingTeamWinner(
	APdPlayerState*& OutWinnerPlayerState,
	const int32 TopKillCount,
	const APlayerState* ExcludedPlayerState) const
{
	OutWinnerPlayerState = nullptr;

	const AExperienceGameMode* GameMode =
		GetExperienceGameModeConst();
	const AGameStateBase* CurrentGameState =
		GameMode
			? GameMode->GetGameState<AGameStateBase>()
			: nullptr;
	if (!CurrentGameState)
	{
		return false;
	}

	TArray<int32> LeaderTeamColorIndices;
	for (APlayerState* PlayerState : CurrentGameState->PlayerArray)
	{
		if (PlayerState == ExcludedPlayerState)
		{
			continue;
		}

		APdPlayerState* PdPlayerState = Cast<APdPlayerState>(PlayerState);
		if (!PdPlayerState
			|| FMath::Max(
				FMath::RoundToInt(PdPlayerState->GetScore()),
				0) != TopKillCount)
		{
			continue;
		}

		if (!OutWinnerPlayerState)
		{
			OutWinnerPlayerState = PdPlayerState;
		}
		LeaderTeamColorIndices.Add(
			PdPlayerState->GetPlayerMatchComponent()
				->GetMatchTeamColorIndex());
	}

	if (!OutWinnerPlayerState
		|| ShouldEnterGoldenKillForLeaderTeams(
			LeaderTeamColorIndices))
	{
		OutWinnerPlayerState = nullptr;
		return false;
	}

	return LeaderTeamColorIndices.Num() >= 2;
}

bool UExperienceMatchFlowComponent::FindTopKiller(
	APdPlayerState*& OutTopKillerPlayerState,
	int32& OutTopKillCount) const
{
	OutTopKillerPlayerState = nullptr;
	OutTopKillCount = 0;

	const AExperienceGameMode* GameMode =
		GetExperienceGameModeConst();
	const AGameStateBase* CurrentGameState =
		GameMode
			? GameMode->GetGameState<AGameStateBase>()
			: nullptr;
	if (!CurrentGameState)
	{
		return false;
	}

	int32 TopKillCount = MIN_int32;
	for (APlayerState* PlayerState : CurrentGameState->PlayerArray)
	{
		APdPlayerState* PdPlayerState =
			Cast<APdPlayerState>(PlayerState);
		if (!PdPlayerState)
		{
			continue;
		}

		const int32 KillCount = FMath::Max(
			FMath::RoundToInt(PdPlayerState->GetScore()),
			0);
		if (!OutTopKillerPlayerState || KillCount > TopKillCount)
		{
			TopKillCount = KillCount;
			OutTopKillerPlayerState = PdPlayerState;
		}
	}

	if (!OutTopKillerPlayerState)
	{
		return false;
	}

	OutTopKillCount = FMath::Max(TopKillCount, 0);
	return true;
}

bool UExperienceMatchFlowComponent::ShouldAbortMatchForPlayerExit(
	const APlayerState* ExitingPlayerState) const
{
	const AExperienceGameMode* GameMode =
		GetExperienceGameModeConst();
	const UExperiencePlayerProvisioningComponent* Provisioning =
		GameMode
			? GameMode->GetPlayerProvisioningComponent()
			: nullptr;
	if (!GameMode
		|| !GameMode->HasAuthority()
		|| bGameResultShown
		|| !ExitingPlayerState
		|| (Provisioning && Provisioning->IsTrainingRoomMap()))
	{
		return false;
	}

	const UWorld* World = GetWorld();
	const AGameStateBase* CurrentGameState =
		GameMode->GetGameState<AGameStateBase>();
	return World
		&& World->GetNetMode() != NM_Standalone
		&& CurrentGameState
		&& CurrentGameState->PlayerArray.Num() > 1;
}

void UExperienceMatchFlowComponent::ReturnToLobbyAfterGameResult()
{
	GameResultLobbyReturnTimerHandle.Invalidate();
	AExperienceGameMode* GameMode = GetExperienceGameMode();
	UWorld* World = GetWorld();
	if (!GameMode
		|| !GameMode->HasAuthority()
		|| !bGameResultShown
		|| !World)
	{
		return;
	}

	const FString LobbyMapName = GetResolvedLobbyTravelMapName();
	if (!LobbyMapName.IsEmpty())
	{
		World->ServerTravel(LobbyMapName);
	}
}

FString UExperienceMatchFlowComponent::GetResolvedTitleTravelMapName() const
{
	const ULevelDefinition* Definition =
		ULevelDefinition::ResolveDefaultDefinition();
	return Definition ? Definition->GetTitleTravelMapName() : FString();
}

FString UExperienceMatchFlowComponent::GetResolvedLobbyTravelMapName() const
{
	const ULevelDefinition* Definition =
		ULevelDefinition::ResolveDefaultDefinition();
	return Definition ? Definition->GetLobbyTravelMapName() : FString();
}

FGameResultPresentationData
UExperienceMatchFlowComponent::BuildPlayerExitGameResult(
	const APlayerState* ExitingPlayerState,
	const APlayerState* WinnerPlayerState,
	const int32 WinnerTeamColorIndex,
	const int32 WinnerTeamMemberCount) const
{
	FGameResultPresentationData GameResultData;
	GameResultData.WinnerTitle = NSLOCTEXT(
		"GameResult",
		"MatchEndedByPlayerExit",
		"Match Ended Due to Player Leaving");
	GameResultData.WinnerTeamColorIndex = WinnerTeamColorIndex;
	GameResultData.bAllowLobbyTravelOnExit = false;
	GameResultData.bShowRewards = WinnerPlayerState != nullptr;

	BuildGameResultPlayerStats(GameResultData.PlayerStats);
	if (WinnerPlayerState)
	{
		ApplyVictoryRewardEligibility(
			GameResultData.PlayerStats,
			WinnerPlayerState,
			WinnerTeamColorIndex,
			WinnerTeamMemberCount,
			ExitingPlayerState);
	}

	if (!GameResultData.PlayerStats.IsEmpty())
	{
		GameResultData.MaxKillerName =
			GameResultData.PlayerStats[0].PlayerName;
		GameResultData.MaxKillCount =
			GameResultData.PlayerStats[0].KillCount;
	}
	else
	{
		GameResultData.MaxKillerName =
			ResolveResultPlayerName(ExitingPlayerState);
	}

	return GameResultData;
}

void UExperienceMatchFlowComponent::SendPlayerExitGameResultToTitle(
	const FGameResultPresentationData& GameResultData,
	const APlayerState* ExitingPlayerState)
{
	UWorld* World = GetWorld();
	const FString TitleMapName = GetResolvedTitleTravelMapName();
	if (!World || TitleMapName.IsEmpty())
	{
		return;
	}

	for (FConstPlayerControllerIterator Iterator =
			World->GetPlayerControllerIterator();
		Iterator;
		++Iterator)
	{
		APlayerController* PlayerController = Iterator->Get();
		if (!PlayerController
			|| PlayerController->PlayerState == ExitingPlayerState)
		{
			continue;
		}

		if (APdPlayerController* PdPlayerController =
			Cast<APdPlayerController>(PlayerController))
		{
			PdPlayerController->Client_TravelToTitleWithGameResult(
				GameResultData,
				TitleMapName);
		}
		else
		{
			PlayerController->ClientTravel(
				TitleMapName,
				TRAVEL_Absolute);
		}
	}
}

AController* UExperienceMatchFlowComponent::FindControllerForPlayerState(
	const APlayerState* PlayerState) const
{
	UWorld* World = GetWorld();
	if (!PlayerState || !World)
	{
		return nullptr;
	}

	for (FConstControllerIterator Iterator =
			World->GetControllerIterator();
		Iterator;
		++Iterator)
	{
		AController* Controller = Iterator->Get();
		if (Controller && Controller->PlayerState == PlayerState)
		{
			return Controller;
		}
	}

	return nullptr;
}

int32 UExperienceMatchFlowComponent::CountPlayersOnTeam(
	const int32 TeamColorIndex,
	const APlayerState* ExcludedPlayerState) const
{
	if (TeamColorIndex == INDEX_NONE)
	{
		return 1;
	}

	const AExperienceGameMode* GameMode =
		GetExperienceGameModeConst();
	const AGameStateBase* CurrentGameState =
		GameMode
			? GameMode->GetGameState<AGameStateBase>()
			: nullptr;
	if (!CurrentGameState)
	{
		return 1;
	}

	int32 TeamMemberCount = 0;
	for (APlayerState* PlayerState : CurrentGameState->PlayerArray)
	{
		if (PlayerState == ExcludedPlayerState)
		{
			continue;
		}

		const APdPlayerState* PdPlayerState =
			Cast<APdPlayerState>(PlayerState);
		if (PdPlayerState
			&& PdPlayerState->GetPlayerMatchComponent()
				->GetMatchTeamColorIndex() == TeamColorIndex)
		{
			++TeamMemberCount;
		}
	}

	return FMath::Max(TeamMemberCount, 1);
}

int32 UExperienceMatchFlowComponent::GrantVictoryRewardsForWinner(
	APlayerState* WinnerPlayerState,
	const int32 WinnerTeamColorIndex,
	const int32 WinnerTeamMemberCount,
	const APlayerState* ExcludedPlayerState)
{
	if (!WinnerPlayerState
		|| WinnerPlayerState == ExcludedPlayerState)
	{
		return 0;
	}

	AExperienceGameMode* GameMode = GetExperienceGameMode();
	int32 RewardedWinnerCount = 0;
	const AGameStateBase* CurrentGameState =
		GameMode
			? GameMode->GetGameState<AGameStateBase>()
			: nullptr;
	if (WinnerTeamColorIndex != INDEX_NONE
		&& CurrentGameState)
	{
		for (APlayerState* PlayerState :
			CurrentGameState->PlayerArray)
		{
			if (PlayerState == ExcludedPlayerState)
			{
				continue;
			}

			const APdPlayerState* PdPlayerState =
				Cast<APdPlayerState>(PlayerState);
			if (!PdPlayerState
				|| PdPlayerState->GetPlayerMatchComponent()
					->GetMatchTeamColorIndex()
					!= WinnerTeamColorIndex)
			{
				continue;
			}

			if (AController* TeamWinnerController =
				FindControllerForPlayerState(PdPlayerState))
			{
				GrantGameVictoryGoldReward(
					TeamWinnerController,
					WinnerTeamMemberCount);
				++RewardedWinnerCount;
			}
		}
	}

	if (RewardedWinnerCount <= 0)
	{
		AController* WinnerController =
			FindControllerForPlayerState(WinnerPlayerState);
		GrantGameVictoryGoldReward(
			WinnerController,
			WinnerTeamMemberCount);
		RewardedWinnerCount = WinnerController ? 1 : 0;
	}

	return RewardedWinnerCount;
}

void UExperienceMatchFlowComponent::ApplyVictoryRewardEligibility(
	TArray<FGameResultPlayerStat>& PlayerStats,
	const APlayerState* WinnerPlayerState,
	const int32 WinnerTeamColorIndex,
	const int32 WinnerTeamMemberCount,
	const APlayerState* ExcludedPlayerState) const
{
	const int32 ExcludedPlayerStateId = ExcludedPlayerState
		? ExcludedPlayerState->GetPlayerId()
		: INDEX_NONE;
	const FText ExcludedPlayerName =
		ResolveResultPlayerName(ExcludedPlayerState);
	for (FGameResultPlayerStat& PlayerStat : PlayerStats)
	{
		const bool bIsExcludedPlayer = ExcludedPlayerState
			&& ((ExcludedPlayerStateId != INDEX_NONE
				&& PlayerStat.PlayerStateId == ExcludedPlayerStateId)
				|| (ExcludedPlayerStateId == INDEX_NONE
					&& PlayerStat.PlayerName.EqualTo(
						ExcludedPlayerName)));
		const bool bIsWinningTeamMember =
			WinnerTeamColorIndex != INDEX_NONE
			&& PlayerStat.TeamColorIndex == WinnerTeamColorIndex;
		const bool bIsFallbackWinner =
			WinnerTeamColorIndex == INDEX_NONE
			&& PlayerStat.PlayerName.EqualTo(
				ResolveResultPlayerName(WinnerPlayerState));
		PlayerStat.bVictoryRewardEligible =
			!bIsExcludedPlayer
			&& (bIsWinningTeamMember || bIsFallbackWinner);
		PlayerStat.GoldReward = PlayerStat.bVictoryRewardEligible
			? CalculateVictoryGoldReward(
				PlayerStat.KillCount,
				PlayerStat.DeathCount,
				WinnerTeamMemberCount)
			: 0;
	}
}

int32 UExperienceMatchFlowComponent::CalculateVictoryGoldReward(
	const APdPlayerState* PlayerState,
	const int32 WinningTeamMemberCount) const
{
	return PlayerState
		? CalculateVictoryGoldReward(
			PlayerState->GetPlayerMatchComponent()->GetKillCount(),
			PlayerState->GetPlayerMatchComponent()->GetDeathCount(),
			WinningTeamMemberCount)
		: 0;
}

FText UExperienceMatchFlowComponent::ResolveResultPlayerName(
	const APlayerState* PlayerState) const
{
	if (!PlayerState)
	{
		return NSLOCTEXT(
			"GameResult",
			"UnknownPlayerName",
			"Unknown");
	}

	if (const APdPlayerState* PdPlayerState =
		Cast<APdPlayerState>(PlayerState))
	{
		const FText DisplayName =
			PdPlayerState->GetPlayerMatchComponent()
				->GetMatchDisplayName();
		if (!DisplayName.IsEmpty())
		{
			return DisplayName;
		}
	}

	const FString PlayerName = PlayerState->GetPlayerName();
	return FText::FromString(
		PlayerName.IsEmpty()
			? GetNameSafe(PlayerState)
			: PlayerName);
}

FText UExperienceMatchFlowComponent::ResolveResultTeamName(
	const int32 TeamColorIndex) const
{
	switch (TeamColorIndex)
	{
	case 0:
		return NSLOCTEXT("GameResult", "TeamNameRed", "Red");
	case 1:
		return NSLOCTEXT("GameResult", "TeamNameBlue", "Blue");
	case 2:
		return NSLOCTEXT("GameResult", "TeamNameYellow", "Yellow");
	case 3:
		return NSLOCTEXT("GameResult", "TeamNamePurple", "Purple");
	case 4:
		return NSLOCTEXT("GameResult", "TeamNameGreen", "Green");
	case 5:
		return NSLOCTEXT("GameResult", "TeamNameOrange", "Orange");
	default:
		return NSLOCTEXT("GameResult", "TeamNameNone", "No Team");
	}
}

FText UExperienceMatchFlowComponent::ResolveWinnerTeamTitle(
	const APlayerState* WinnerPlayerState) const
{
	const APdPlayerState* WinnerPdPlayerState =
		Cast<APdPlayerState>(WinnerPlayerState);
	const FText TeamName = WinnerPdPlayerState
		? ResolveResultTeamName(
			WinnerPdPlayerState->GetPlayerMatchComponent()
				->GetMatchTeamColorIndex())
		: NSLOCTEXT(
			"GameResult",
			"UnknownTeamName",
			"Unknown");
	return FText::Format(
		NSLOCTEXT(
			"GameResult",
			"WinnerTeamTitleFormat",
			"{0} Team Wins"),
		TeamName);
}

void UExperienceMatchFlowComponent::BuildGameResultPlayerStats(
	TArray<FGameResultPlayerStat>& OutPlayerStats) const
{
	OutPlayerStats.Reset();

	const AExperienceGameMode* GameMode =
		GetExperienceGameModeConst();
	const AGameStateBase* CurrentGameState =
		GameMode
			? GameMode->GetGameState<AGameStateBase>()
			: nullptr;
	if (!CurrentGameState)
	{
		return;
	}

	for (APlayerState* PlayerState : CurrentGameState->PlayerArray)
	{
		const APdPlayerState* PdPlayerState =
			Cast<APdPlayerState>(PlayerState);
		if (!PdPlayerState)
		{
			continue;
		}

		const UPlayerMatchComponent* MatchComponent =
			PdPlayerState->GetPlayerMatchComponent();
		FGameResultPlayerStat PlayerStat;
		PlayerStat.PlayerName = ResolveResultPlayerName(PdPlayerState);
		PlayerStat.TeamColorIndex =
			MatchComponent->GetMatchTeamColorIndex();
		PlayerStat.PlayerStateId = PdPlayerState->GetPlayerId();
		PlayerStat.TeamName =
			ResolveResultTeamName(PlayerStat.TeamColorIndex);
		PlayerStat.KillCount = MatchComponent->GetKillCount();
		PlayerStat.DeathCount = MatchComponent->GetDeathCount();
		OutPlayerStats.Add(PlayerStat);
	}

	OutPlayerStats.Sort(
		[](const FGameResultPlayerStat& A,
			const FGameResultPlayerStat& B)
		{
			if (A.KillCount != B.KillCount)
			{
				return A.KillCount > B.KillCount;
			}
			if (A.DeathCount != B.DeathCount)
			{
				return A.DeathCount < B.DeathCount;
			}
			return A.PlayerName.ToString() < B.PlayerName.ToString();
		});
}

void UExperienceMatchFlowComponent::ForceMovePlayersForGoldenKill()
{
	const AExperienceGameMode* GameMode =
		GetExperienceGameModeConst();
	if (GameMode)
	{
		if (UExperienceSpawnComponent* SpawnComponent =
			GameMode->GetSpawnComponent())
		{
			SpawnComponent->ForceMovePlayersToInitialSpawns();
		}
	}
	RaiseForceMoveGatesForGoldenKill();
}

void UExperienceMatchFlowComponent::RaiseForceMoveGatesForGoldenKill()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<AForceMoveGateActor> Iterator(World);
		Iterator;
		++Iterator)
	{
		AForceMoveGateActor* GateActor = *Iterator;
		if (GateActor
			&& GateActor->ShouldRaiseWhenForceMoveTriggered())
		{
			GateActor->HandleForceMoveTriggered(nullptr);
		}
	}
}

void UExperienceMatchFlowComponent::StartGoldenKill(
	const int32 TopKillCount)
{
	const AExperienceGameMode* GameMode =
		GetExperienceGameModeConst();
	if (GameMode
		&& GameMode->HasAuthority()
		&& !bGameResultShown)
	{
		GoldenKillVictoryScore =
			CalculateGoldenKillVictoryScore(TopKillCount);
		bGoldenKillActive = true;
		RestorePlayerResourcesForGoldenKill();
	}
}

void UExperienceMatchFlowComponent::RestorePlayerResourcesForGoldenKill() const
{
	const AExperienceGameMode* GameMode =
		GetExperienceGameModeConst();
	const AGameStateBase* CurrentGameState = GameMode
		? GameMode->GetGameState<AGameStateBase>()
		: nullptr;
	if (!CurrentGameState)
	{
		return;
	}

	for (APlayerState* PlayerState : CurrentGameState->PlayerArray)
	{
		const APdPlayerState* PdPlayerState =
			Cast<APdPlayerState>(PlayerState);
		UPdAbilitySystemComponent* AbilitySystemComponent =
			PdPlayerState
				? PdPlayerState->GetPdAbilitySystemComponent()
				: nullptr;
		if (!AbilitySystemComponent
			|| !AbilitySystemComponent->GetAttributeSet(
				UBasicAttributeSet::StaticClass()))
		{
			continue;
		}

		AbilitySystemComponent->SetNumericAttributeBase(
			UBasicAttributeSet::GetHealthAttribute(),
			AbilitySystemComponent->GetNumericAttribute(
				UBasicAttributeSet::GetMaxHealthAttribute()));
		AbilitySystemComponent->SetNumericAttributeBase(
			UBasicAttributeSet::GetManaAttribute(),
			AbilitySystemComponent->GetNumericAttribute(
				UBasicAttributeSet::GetMaxManaAttribute()));
		AbilitySystemComponent->SetNumericAttributeBase(
			UBasicAttributeSet::GetStaminaAttribute(),
			AbilitySystemComponent->GetNumericAttribute(
				UBasicAttributeSet::GetMaxStaminaAttribute()));
		AbilitySystemComponent->ForceReplication();
	}
}

const URewardDefinition*
UExperienceMatchFlowComponent::ResolveRewardDefinitionForChestSpawns(
	const TArray<ARewardChest*>& RewardChests) const
{
	if (!Settings.ChestSpawnRewardDefinition.IsNull())
	{
		if (const URewardDefinition* RewardDefinition =
			Settings.ChestSpawnRewardDefinition.Get())
		{
			return RewardDefinition;
		}
	}

	for (const ARewardChest* RewardChest : RewardChests)
	{
		if (!IsValid(RewardChest))
		{
			continue;
		}

		const TSoftObjectPtr<URewardDefinition> RewardDefinitionAsset =
			RewardChest->GetRewardDefinitionAsset();
		if (!RewardDefinitionAsset.IsNull())
		{
			if (const URewardDefinition* RewardDefinition =
				RewardDefinitionAsset.Get())
			{
				return RewardDefinition;
			}
		}
	}

	return nullptr;
}

void UExperienceMatchFlowComponent::BeginRuntimeContentPreload()
{
	TSet<FSoftObjectPath> AssetPaths;
	const auto AddSoftPath = [&AssetPaths](const auto& SoftObject)
	{
		if (!SoftObject.IsNull())
		{
			AssetPaths.Add(SoftObject.ToSoftObjectPath());
		}
	};

	AddSoftPath(Settings.GameVictoryRewardDefinition);
	AddSoftPath(Settings.ChestSpawnRewardDefinition);
	AddSoftPath(Settings.MatchRuleDefinition);
	AddSoftPath(Settings.LevelDefinition);

	if (AssetPaths.IsEmpty())
	{
		bRuntimeContentLoadPending = false;
		ResumePendingInitialization();
		return;
	}

	bRuntimeContentLoadPending = true;
	const uint32 RequestGeneration = RuntimeContentRequestGeneration;
	RuntimeContentPreloadHandle =
		UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
			AssetPaths.Array(),
			FStreamableDelegate::CreateUObject(
				this,
				&ThisClass::HandleRuntimeContentPreloadComplete,
				RequestGeneration));
	if (!RuntimeContentPreloadHandle.IsValid())
	{
		UE_LOG(
			LogExperienceMatchFlowContent,
			Error,
			TEXT("Experience match-flow content preload could not be started for '%s'."),
			*GetPathNameSafe(GetOwner()));
		bRuntimeContentLoadPending = false;
		ResumePendingInitialization();
	}
}

void UExperienceMatchFlowComponent::HandleRuntimeContentPreloadComplete(
	const uint32 RequestGeneration)
{
	if (RequestGeneration != RuntimeContentRequestGeneration)
	{
		return;
	}

	bRuntimeContentLoadPending = false;
	ResumePendingInitialization();
}

void UExperienceMatchFlowComponent::ReleaseRuntimeContentPreload()
{
	++RuntimeContentRequestGeneration;
	bRuntimeContentLoadPending = false;
	if (RuntimeContentPreloadHandle.IsValid())
	{
		RuntimeContentPreloadHandle->CancelHandle();
		RuntimeContentPreloadHandle->ReleaseHandle();
		RuntimeContentPreloadHandle.Reset();
	}
}

void UExperienceMatchFlowComponent::ResumePendingInitialization()
{
	const bool bShouldInitializeGameState = bInitializeGameStateRequested;
	const bool bShouldStartMatchTimer = bStartMatchTimerRequested;
	const bool bShouldConfigureRewardChests = bConfigureRewardChestsRequested;
	bInitializeGameStateRequested = false;
	bStartMatchTimerRequested = false;
	bConfigureRewardChestsRequested = false;

	if (bShouldInitializeGameState)
	{
		InitializeGameState();
	}
	if (bShouldStartMatchTimer)
	{
		StartServerMatchTimerIfNeeded();
	}
	if (bShouldConfigureRewardChests)
	{
		ConfigureRewardChestSpawns();
	}
}
