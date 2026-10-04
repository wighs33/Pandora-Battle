#include "Component/Match/MatchFlowComponent.h"

#include "Common/GameSessionConstants.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Component/Match/MatchOutcomeRules.h"
#include "Component/Match/MatchPlayerSetupComponent.h"
#include "Component/Match/MatchResultReport.h"
#include "Component/Match/MatchRewardComponent.h"
#include "Component/Match/MatchTravel.h"
#include "Component/Player/PlayerMatchComponent.h"
#include "Component/Player/PlayerSpawnComponent.h"
#include "Definition/Level/LevelDefinition.h"
#include "Definition/Match/MatchRuleDefinition.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Map/ForceMoveGateActor.h"
#include "Mode/ExperienceGameMode.h"
#include "Mode/ExperienceGameState.h"
#include "Mode/PdPlayerController.h"
#include "Mode/PdPlayerState.h"
#include "Mode/PlayerEliminationSubsystem.h"
#include "Online/GameLift/GameLiftServerSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MatchFlowComponent)

namespace
{
	constexpr float GameResultLobbyReturnDelaySeconds = 5.0f;

	bool IsEnabledTravelOption(const FString& Options, const TCHAR* OptionName)
	{
		const FString OptionKey(OptionName);
		if (!UGameplayStatics::HasOption(Options, OptionKey))
		{
			return false;
		}

		FString OptionValue = UGameplayStatics::ParseOption(Options, OptionKey);
		OptionValue.TrimStartAndEndInline();
		return OptionValue.Equals(TEXT("1"), ESearchCase::IgnoreCase)
			|| OptionValue.Equals(TEXT("true"), ESearchCase::IgnoreCase)
			|| OptionValue.Equals(TEXT("yes"), ESearchCase::IgnoreCase);
	}

	/** 판정 대상 참가자와 그 기록, 판정 결과. 같은 인덱스끼리 짝을 이룬다. */
	struct FMatchSnapshot
	{
		TArray<APdPlayerState*> Players;
		TArray<FMatchStanding> Standings;
		FMatchOutcome Outcome;

		APdPlayerState* GetWinner() const
		{
			return Outcome.HasWinner() ? Players[Outcome.WinnerIndex] : nullptr;
		}

		int32 GetWinnerTeamColorIndex() const
		{
			return Outcome.HasWinner() ? Standings[Outcome.WinnerIndex].TeamColorIndex : INDEX_NONE;
		}

		int32 GetWinnerTeamMemberCount() const
		{
			return MatchOutcomeRules::CountTeamMembers(Standings, GetWinnerTeamColorIndex());
		}

		APdPlayerState* GetTopScorer() const
		{
			return Outcome.TopScorerIndex != INDEX_NONE ? Players[Outcome.TopScorerIndex] : nullptr;
		}
	};

	FMatchSnapshot TakeMatchSnapshot(const AGameStateBase* GameState, const APlayerState* ExcludedPlayerState = nullptr)
	{
		FMatchSnapshot Snapshot;
		if (GameState)
		{
			for (APlayerState* PlayerState : GameState->PlayerArray)
			{
				APdPlayerState* PdPlayerState = Cast<APdPlayerState>(PlayerState);
				if (!PdPlayerState || PlayerState == ExcludedPlayerState)
				{
					continue;
				}

				const UPlayerMatchComponent* MatchComponent = PdPlayerState->GetPlayerMatchComponent();
				Snapshot.Players.Add(PdPlayerState);
				Snapshot.Standings.Add({MatchComponent->GetKillCount(), MatchComponent->GetMatchTeamColorIndex()});
			}
		}
		Snapshot.Outcome = MatchOutcomeRules::Resolve(Snapshot.Standings);
		return Snapshot;
	}
}

UMatchFlowComponent::UMatchFlowComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMatchFlowComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UPlayerEliminationSubsystem* Eliminations = UWorld::GetSubsystem<UPlayerEliminationSubsystem>(GetWorld()))
	{
		KillScoredHandle = Eliminations->OnPlayerKillScored.AddUObject(this, &ThisClass::HandlePlayerKillScored);
	}
}

void UMatchFlowComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UPlayerEliminationSubsystem* Eliminations = UWorld::GetSubsystem<UPlayerEliminationSubsystem>(GetWorld()))
	{
		Eliminations->OnPlayerKillScored.Remove(KillScoredHandle);
	}
	KillScoredHandle.Reset();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}
	MatchTimerHandle.Invalidate();
	GameResultLobbyReturnTimerHandle.Invalidate();

	Super::EndPlay(EndPlayReason);
}

AExperienceGameMode* UMatchFlowComponent::GetExperienceGameMode() const
{
	return Cast<AExperienceGameMode>(GetOwner());
}

void UMatchFlowComponent::InitializeTravelOptions(const FString& Options)
{
	bMatchTimerSuppressedByTravelOption = IsEnabledTravelOption(Options, LabGameSession::NoMatchTimerOption);
	bBossRaid = IsEnabledTravelOption(Options, LabGameSession::BossRaidOption);
}

void UMatchFlowComponent::InitializeGameState()
{
	AExperienceGameMode* GameMode = GetExperienceGameMode();
	AExperienceGameState* ExperienceGameState = GameMode ? GameMode->GetGameState<AExperienceGameState>() : nullptr;
	if (!ExperienceGameState)
	{
		return;
	}

	ExperienceGameState->SetMatchRuleDefinition(GameMode->GetMatchRuleDefinition());
	ExperienceGameState->SetMatchTimerState(
		ShouldSuppressServerMatchTimer() ? EMatchTimerPhase::Suppressed : EMatchTimerPhase::Inactive);
}

// GameMode의 준비 판정 이후 한 번만 시작한다. 중복 요청이나 늦은 입장으로 종료 시각을 갱신하지 않는다.
void UMatchFlowComponent::StartServerMatchTimerIfNeeded()
{
	AExperienceGameMode* GameMode = GetExperienceGameMode();
	UWorld* World = GetWorld();
	AExperienceGameState* ExperienceGameState = GameMode ? GameMode->GetGameState<AExperienceGameState>() : nullptr;
	if (!GameMode
		|| !GameMode->IsRuntimeContentReady()
		|| bServerMatchTimerStarted
		|| bGameResultShown
		|| !GameMode->HasAuthority()
		|| !World
		|| !ExperienceGameState)
	{
		return;
	}
	bServerMatchTimerStarted = true;

	if (ShouldSuppressServerMatchTimer())
	{
		ExperienceGameState->SetMatchTimerState(EMatchTimerPhase::Suppressed);
		return;
	}

	const float MatchTimerSeconds = GameMode->GetMatchRuleDefinition()->MatchTimerSeconds;
	if (MatchTimerSeconds <= 0.0f)
	{
		HandleMatchTimerExpired();
		return;
	}

	ExperienceGameState->SetMatchTimerState(
		EMatchTimerPhase::Running, ExperienceGameState->GetServerWorldTimeSeconds() + MatchTimerSeconds);
	World->GetTimerManager().SetTimer(MatchTimerHandle, this, &ThisClass::HandleMatchTimerExpired, MatchTimerSeconds, false);
}

void UMatchFlowComponent::HandleMatchTimerExpired()
{
	AExperienceGameMode* GameMode = GetExperienceGameMode();
	if (!GameMode || !GameMode->HasAuthority() || bMatchTimerExpired)
	{
		return;
	}

	bMatchTimerExpired = true;
	GetWorld()->GetTimerManager().ClearTimer(MatchTimerHandle);
	if (AExperienceGameState* ExperienceGameState = GameMode->GetGameState<AExperienceGameState>())
	{
		ExperienceGameState->SetMatchTimerState(EMatchTimerPhase::Expired);
	}

	// 같은 팀끼리의 동점은 이미 팀 승리다. 골든킬은 서로 다른 팀이 최고 점수를 나눠 가졌을 때만 연다.
	const FMatchSnapshot Snapshot = TakeMatchSnapshot(GameMode->GetGameState<AGameStateBase>());
	if (APdPlayerState* WinnerPlayerState = Snapshot.GetWinner())
	{
		ShowGameResult(
			WinnerPlayerState,
			Snapshot.GetWinnerTeamMemberCount(),
			Snapshot.GetTopScorer(),
			Snapshot.Outcome.TopScore);
		return;
	}

	const UMatchRuleDefinition* MatchRules = GameMode->GetMatchRuleDefinition();
	if (Snapshot.Outcome.Type == EMatchOutcomeType::OpposingTie && MatchRules && MatchRules->bGoldenKillEnabled)
	{
		StartGoldenKill();
	}
}

// 골든킬 중에는 처치로 단독 1위가 된 플레이어가 바로 이긴다.
void UMatchFlowComponent::HandlePlayerKillScored(APlayerState* KillerPlayerState, APlayerState* VictimPlayerState)
{
	const AExperienceGameMode* GameMode = GetExperienceGameMode();
	if (!GameMode
		|| !GameMode->HasAuthority()
		|| !bGoldenKillActive
		|| bGameResultShown
		|| !KillerPlayerState
		|| KillerPlayerState == VictimPlayerState)
	{
		return;
	}

	const FMatchSnapshot Snapshot = TakeMatchSnapshot(GameMode->GetGameState<AGameStateBase>());
	if (Snapshot.Outcome.Type == EMatchOutcomeType::UniqueLeader && Snapshot.GetWinner() == KillerPlayerState)
	{
		ShowGameResult(
			Snapshot.GetWinner(),
			Snapshot.GetWinnerTeamMemberCount(),
			Snapshot.GetTopScorer(),
			Snapshot.Outcome.TopScore);
	}
}

// 모든 참가자의 자원을 채우고 처음 스폰 위치로 옮긴 뒤, 이후 리스폰도 처음 위치에서 하게 한다.
void UMatchFlowComponent::StartGoldenKill()
{
	const AExperienceGameMode* GameMode = GetExperienceGameMode();
	if (!GameMode || !GameMode->HasAuthority() || bGameResultShown)
	{
		return;
	}

	bGoldenKillActive = true;
	UPlayerSpawnComponent* SpawnComponent = GameMode->GetSpawnComponent();
	SpawnComponent->SetRespawnLocation(EPlayerRespawnLocation::InitialSpawn);

	if (const AGameStateBase* GameState = GameMode->GetGameState<AGameStateBase>())
	{
		for (APlayerState* PlayerState : GameState->PlayerArray)
		{
			const APdPlayerState* PdPlayerState = Cast<APdPlayerState>(PlayerState);
			if (UPdAbilitySystemComponent* AbilitySystem = PdPlayerState
				? Cast<UPdAbilitySystemComponent>(PdPlayerState->GetAbilitySystemComponent())
				: nullptr)
			{
				AbilitySystem->RestoreResourcesToMaximum();
			}
		}
	}

	for (APlayerController* Player : SpawnComponent->MovePlayersToInitialSpawns())
	{
		if (APdPlayerController* PdPlayerController = Cast<APdPlayerController>(Player))
		{
			PdPlayerController->Client_ShowGoldenKillAnnouncement(
				NSLOCTEXT("GoldenKill", "GoldenKillAnnouncement", "GOLDEN KILL"));
		}
	}

	// 강제 이동에 반응하도록 설정된 관문도 함께 올린다.
	for (TActorIterator<AForceMoveGateActor> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		if (Iterator->ShouldRaiseWhenForceMoveTriggered())
		{
			Iterator->HandleForceMoveTriggered(nullptr);
		}
	}
}

void UMatchFlowComponent::ShowGameResult(
	APdPlayerState* WinnerPlayerState,
	const int32 WinnerTeamMemberCount,
	const APdPlayerState* TopScorerPlayerState,
	const int32 TopScore)
{
	AExperienceGameMode* GameMode = GetExperienceGameMode();
	AExperienceGameState* ExperienceGameState = GameMode ? GameMode->GetGameState<AExperienceGameState>() : nullptr;
	if (!GameMode || !GameMode->HasAuthority() || !WinnerPlayerState || bGameResultShown || !ExperienceGameState)
	{
		return;
	}

	FinishMatchRuntime();
	ExperienceGameState->SetMatchTimerState(EMatchTimerPhase::Expired);

	const int32 WinnerTeamColorIndex = WinnerPlayerState->GetPlayerMatchComponent()->GetMatchTeamColorIndex();
	GameMode->GetRewardComponent()->GrantVictoryGold(WinnerPlayerState, WinnerTeamColorIndex, WinnerTeamMemberCount);

	TArray<FGameResultPlayerStat> PlayerStats = MatchResultReport::BuildPlayerStats(*ExperienceGameState);
	MatchResultReport::ApplyVictoryRewards(
		PlayerStats,
		WinnerPlayerState,
		WinnerTeamColorIndex,
		WinnerTeamMemberCount,
		nullptr,
		GameMode->GetVictoryGoldRates());
	MatchResultReport::ReportToBackend(*GameMode, WinnerPlayerState, WinnerTeamColorIndex, TEXT("completed"));

	ExperienceGameState->Multicast_ShowGameResult(
		MatchResultReport::ResolveWinnerTitle(WinnerTeamColorIndex),
		WinnerTeamColorIndex,
		UPlayerMatchComponent::ResolveDisplayName(TopScorerPlayerState ? TopScorerPlayerState : WinnerPlayerState),
		TopScore,
		PlayerStats);

	GetWorld()->GetTimerManager().SetTimer(
		GameResultLobbyReturnTimerHandle,
		this,
		&ThisClass::ReturnToLobbyAfterGameResult,
		GameResultLobbyReturnDelaySeconds,
		false);
}

bool UMatchFlowComponent::RequestAbortMatchToTitle(APlayerController* RequestingPlayer)
{
	const AExperienceGameMode* GameMode = GetExperienceGameMode();
	if (!GameMode
		|| !GameMode->HasAuthority()
		|| !RequestingPlayer
		// Only the listen host may intentionally end the whole match. Remote
		// players leave locally and are handled by Logout on the server.
		|| !RequestingPlayer->IsLocalController()
		|| !AbortMatchToTitleForPlayerExit(RequestingPlayer->PlayerState))
	{
		return false;
	}

	MatchTravel::SendPlayerToTitle(*RequestingPlayer, GetTitleMapName());
	return true;
}

bool UMatchFlowComponent::HandlePlayerLogout(const APlayerState* ExitingPlayerState)
{
	const AExperienceGameMode* GameMode = GetExperienceGameMode();
	return GameMode
		&& GameMode->HasAuthority()
		&& ExitingPlayerState
		&& AbortMatchToTitleForPlayerExit(ExitingPlayerState);
}

// 남은 참가자끼리 판정해 보상과 결과를 들려 타이틀로 보낸다. 서로 다른 팀이 동점이면 승자 없이 끝난다.
bool UMatchFlowComponent::AbortMatchToTitleForPlayerExit(const APlayerState* ExitingPlayerState)
{
	AExperienceGameMode* GameMode = GetExperienceGameMode();
	if (!GameMode || !ShouldAbortMatchForPlayerExit(ExitingPlayerState))
	{
		return false;
	}

	FinishMatchRuntime();
	if (AExperienceGameState* ExperienceGameState = GameMode->GetGameState<AExperienceGameState>())
	{
		ExperienceGameState->SetMatchTimerState(EMatchTimerPhase::Expired);
	}

	const AGameStateBase* GameState = GameMode->GetGameState<AGameStateBase>();
	const FMatchSnapshot Snapshot = TakeMatchSnapshot(GameState, ExitingPlayerState);
	const APdPlayerState* WinnerPlayerState = Snapshot.GetWinner();
	const int32 WinnerTeamColorIndex = Snapshot.GetWinnerTeamColorIndex();
	const int32 WinnerTeamMemberCount = Snapshot.GetWinnerTeamMemberCount();
	GameMode->GetRewardComponent()->GrantVictoryGold(
		WinnerPlayerState,
		WinnerTeamColorIndex,
		WinnerTeamMemberCount,
		ExitingPlayerState);

	MatchTravel::SendPlayersToTitleWithResult(
		*GetWorld(),
		GetTitleMapName(),
		MatchResultReport::BuildPlayerExitResult(
			*GameState,
			ExitingPlayerState,
			WinnerPlayerState,
			WinnerTeamColorIndex,
			WinnerTeamMemberCount,
			GameMode->GetVictoryGoldRates()),
		ExitingPlayerState);
	MatchResultReport::ReportToBackend(
		*GameMode,
		WinnerPlayerState,
		WinnerTeamColorIndex,
		TEXT("player_exit"),
		ExitingPlayerState);

	// 남은 참가자는 타이틀로 이동했다. GameLift 게임 세션은 결과 보고와 퇴장이 끝나면 종료한다.
	if (UGameLiftServerSubsystem* GameLift = UGameLiftServerSubsystem::Get(this))
	{
		GameLift->RequestSessionEnd(TEXT("Match ended by player exit"));
	}
	return true;
}

bool UMatchFlowComponent::ShouldAbortMatchForPlayerExit(const APlayerState* ExitingPlayerState) const
{
	const AExperienceGameMode* GameMode = GetExperienceGameMode();
	// 보스 레이드 월드는 누가 나가도 남은 플레이어가 계속 머문다.
	if (!GameMode
		|| !GameMode->HasAuthority()
		|| bGameResultShown
		|| bBossRaid
		|| !ExitingPlayerState
		|| !GameMode->IsRuntimeContentReady()
		|| GameMode->GetPlayerSetupComponent()->IsTrainingRoomMap())
	{
		return false;
	}

	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = GameMode->GetGameState<AGameStateBase>();
	return World
		&& World->GetNetMode() != NM_Standalone
		&& GameState
		&& GameState->PlayerArray.Num() > 1;
}

void UMatchFlowComponent::ReturnToLobbyAfterGameResult()
{
	GameResultLobbyReturnTimerHandle.Invalidate();
	const AExperienceGameMode* GameMode = GetExperienceGameMode();
	UWorld* World = GetWorld();
	if (!GameMode || !GameMode->HasAuthority() || !bGameResultShown || !World)
	{
		return;
	}

	// GameLift 게임 세션은 한 경기로 끝난다. 참가자를 타이틀로 보내고, 결과 보고와 퇴장이 끝나면 프로세스를 종료한다.
	UGameLiftServerSubsystem* GameLift = UGameLiftServerSubsystem::Get(this);
	if (GameLift && GameLift->IsGameLiftActive())
	{
		MatchTravel::SendAllPlayersToTitle(*World, GetTitleMapName());
		GameLift->RequestSessionEnd(TEXT("Match finished"));
		return;
	}

	const ULevelDefinition* Levels = GameMode->GetLevelDefinition();
	const FString LobbyMapName = Levels ? Levels->GetLobbyTravelMapName() : FString();
	if (!LobbyMapName.IsEmpty())
	{
		World->ServerTravel(LobbyMapName);
	}
}

void UMatchFlowComponent::FinishMatchRuntime()
{
	bGameResultShown = true;
	bGoldenKillActive = false;
	AExperienceGameMode* GameMode = GetExperienceGameMode();
	GameMode->GetSpawnComponent()->StopRespawning();
	GameMode->GetRewardComponent()->StopChestConfiguration();
	GetWorld()->GetTimerManager().ClearTimer(MatchTimerHandle);
}

// 보스 레이드는 타이머가 끝나지 않으므로 승자 판정·결과·로비 복귀도 일어나지 않는다.
bool UMatchFlowComponent::ShouldSuppressServerMatchTimer() const
{
	if (bMatchTimerSuppressedByTravelOption || bBossRaid)
	{
		return true;
	}

	const UMatchRuleDefinition* MatchRules = GetExperienceGameMode()->GetMatchRuleDefinition();
	return MatchRules
		&& MatchRules->MapsWithoutMatchTimer.Contains(FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
}

FString UMatchFlowComponent::GetTitleMapName() const
{
	const ULevelDefinition* Levels = GetExperienceGameMode()->GetLevelDefinition();
	return Levels ? Levels->GetTitleTravelMapName() : FString();
}
