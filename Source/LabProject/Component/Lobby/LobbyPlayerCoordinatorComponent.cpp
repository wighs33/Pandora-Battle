#include "Component/Lobby/LobbyPlayerCoordinatorComponent.h"
#include "Component/Lobby/LobbyPlayerStateComponent.h"

#include "Component/Lobby/LobbyConfigurationComponent.h"
#include "Component/Player/PlayerMatchComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/GameStateBase.h"
#include "Lobby/Contents/LobbyGameMode.h"
#include "Mode/PdPlayerState.h"
#include "Lobby/Coordination/LobbyMatchCoordinator.h"
#include "Engine/GameInstance.h"
#include "Lobby/LobbyRuntimeSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LobbyPlayerCoordinatorComponent)

ULobbyPlayerCoordinatorComponent::
ULobbyPlayerCoordinatorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void ULobbyPlayerCoordinatorComponent::
InitializeLobbyPlayerState(
	APlayerController* PlayerController,
	APdPlayerState* LobbyPlayerState)
{
	ALobbyGameMode* GameMode = GetLobbyGameMode();
	if (!GameMode || !GameMode->HasAuthority() || !LobbyPlayerState)
	{
		return;
	}

	LobbyPlayerState->SetNetUpdateFrequency(30.0f);
	ULobbyPlayerStateComponent* LobbyState = LobbyPlayerState->GetLobbyPlayerStateComponent();
	LobbyState->SetLeavingLobby(false);

	if (LobbyPlayerState->GetPlayerMatchComponent()->GetMatchDisplayName().IsEmpty())
	{
		++NicknameIndex;
		const ULobbyRuntimeSubsystem* LobbySubsystem =
			UGameInstance::GetSubsystem<ULobbyRuntimeSubsystem>(GameMode->GetGameInstance());
		const FText DefaultNickname = LobbySubsystem
			? LobbySubsystem->ResolveDefaultPlayerNickname(
				PlayerController,
				LobbyPlayerState,
				NicknameIndex)
			: FText::Format(
				NSLOCTEXT(
					"Lobby",
					"DefaultNicknameFormat",
					"User{0}"),
				NicknameIndex);
		LobbyState->SetDefaultNickname(DefaultNickname);
	}
	else
	{
		// 이전 경기에서 인계된 이름을 이번 로비의 입력 힌트로 준비한다.
		LobbyState->SetDefaultNickname(LobbyPlayerState->GetPlayerMatchComponent()->GetMatchDisplayName());
	}

	AssignLobbySpawnIndexIfNeeded(LobbyPlayerState);
	if (ULobbyMatchCoordinator* MatchCoordinator =
		GameMode->GetMatchCoordinator())
	{
		MatchCoordinator
			->AssignLobbyTeamColorIfNeeded(
				LobbyPlayerState);
	}
}

void ULobbyPlayerCoordinatorComponent::KickPlayer(
	APdPlayerState* TargetPlayerState)
{
	ALobbyGameMode* GameMode = GetLobbyGameMode();
	if (!GameMode
		|| !GameMode->HasAuthority()
		|| !TargetPlayerState)
	{
		return;
	}

	APlayerController* TargetPlayerController =
		ResolvePlayerControllerForPlayerState(
			TargetPlayerState);
	if (!TargetPlayerController)
	{
		return;
	}

	ULobbyMatchCoordinator* MatchCoordinator =
		GameMode->GetMatchCoordinator();
	if (MatchCoordinator
		&& MatchCoordinator->IsGameStartRequested())
	{
		MatchCoordinator->CancelPendingGameStart(
			TEXT("player_kicked"));
	}

	TargetPlayerState->GetLobbyPlayerStateComponent()->SetLeavingLobby(true);

	ForceKickPlayer(TargetPlayerController);
}

APlayerController* ULobbyPlayerCoordinatorComponent::
ResolvePlayerControllerForPlayerState(
	const APlayerState* PlayerState) const
{
	if (!PlayerState)
	{
		return nullptr;
	}

	if (APlayerController* PlayerController =
		Cast<APlayerController>(
			PlayerState->GetOwner()))
	{
		return PlayerController;
	}

	const ALobbyGameMode* GameMode =
		GetLobbyGameMode();
	const UWorld* World = GameMode
		? GameMode->GetWorld()
		: nullptr;
	if (!World)
	{
		return nullptr;
	}

	for (FConstPlayerControllerIterator Iterator =
		World->GetPlayerControllerIterator();
		Iterator;
		++Iterator)
	{
		APlayerController* Candidate =
			Iterator->Get();
		if (Candidate
			&& Candidate->PlayerState == PlayerState)
		{
			return Candidate;
		}
	}

	return nullptr;
}

ALobbyGameMode*
ULobbyPlayerCoordinatorComponent::GetLobbyGameMode() const
{
	return Cast<ALobbyGameMode>(GetOwner());
}

void ULobbyPlayerCoordinatorComponent::
AssignLobbySpawnIndexIfNeeded(
	APdPlayerState* LobbyPlayerState) const
{
	if (!LobbyPlayerState
		|| LobbyPlayerState->GetPlayerMatchComponent()
			->GetMatchSpawnIndex() != INDEX_NONE)
	{
		return;
	}

	LobbyPlayerState->GetPlayerMatchComponent()
		->SetMatchSpawnIndex(
			FindAvailableLobbySpawnIndex(
				LobbyPlayerState));
}

int32 ULobbyPlayerCoordinatorComponent::
FindAvailableLobbySpawnIndex(
	const APdPlayerState* IgnoredPlayerState) const
{
	const ALobbyGameMode* GameMode =
		GetLobbyGameMode();
	TSet<int32> UsedSpawnIndices;
	if (GameMode && GameMode->GameState)
	{
		for (APlayerState* PlayerState :
			GameMode->GameState->PlayerArray)
		{
			const APdPlayerState* LobbyPlayerState =
				Cast<APdPlayerState>(PlayerState);
			if (!LobbyPlayerState
				|| LobbyPlayerState
					== IgnoredPlayerState)
			{
				continue;
			}

			const int32 SpawnIndex =
				LobbyPlayerState
					->GetPlayerMatchComponent()
					->GetMatchSpawnIndex();
			if (SpawnIndex != INDEX_NONE)
			{
				UsedSpawnIndices.Add(SpawnIndex);
			}
		}
	}

	const int32 SearchLimit = FMath::Max(
		GameMode
			? GameMode
				->GetLobbyConfigurationComponent()->GetSelectedLobbyMaxPlayerCount()
			: 1,
		1);
	for (int32 SpawnIndex = 0;
		SpawnIndex < SearchLimit;
		++SpawnIndex)
	{
		if (!UsedSpawnIndices.Contains(SpawnIndex))
		{
			return SpawnIndex;
		}
	}

	return UsedSpawnIndices.Num();
}

void ULobbyPlayerCoordinatorComponent::ForceKickPlayer(
	APlayerController* TargetPlayerController)
{
	ALobbyGameMode* GameMode = GetLobbyGameMode();
	if (!GameMode
		|| !GameMode->HasAuthority()
		|| !TargetPlayerController
		|| !GameMode->GameSession)
	{
		return;
	}

	GameMode->GameSession->KickPlayer(
		TargetPlayerController,
		NSLOCTEXT(
			"Lobby",
			"KickedByHost",
			"Kicked by host"));
}
