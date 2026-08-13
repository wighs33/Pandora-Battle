#include "Lobby/Coordination/LobbyTravelCoordinator.h"

#include "Character/PdPlayer.h"
#include "Common/Enum_Direction.h"
#include "Common/GameSessionConstants.h"
#include "Component/Pandora/PandoraComponent.h"
#include "Component/Skin/SkinEquipmentComponent.h"
#include "Definition/Level/LevelDefinition.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "Definition/Skin/SkinDefinition.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Lobby/Contents/LobbyGameMode.h"
#include "Lobby/Contents/LobbyGameState.h"
#include "Lobby/Contents/LobbyPlayerController.h"
#include "Lobby/Contents/LobbyPlayerState.h"
#include "Lobby/Coordination/LobbyMatchCoordinator.h"
#include "Lobby/LobbyRuntimeSubsystem.h"
#include "Mode/PdGameInstance.h"
#include "Online/OnlineSessionsSubsystem.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LobbyTravelCoordinator)

DEFINE_LOG_CATEGORY_STATIC(LogLobbyTravelCoordinator, Log, All);

void ULobbyTravelCoordinator::StartSessionAndTravel()
{
	ALobbyGameMode* GameMode = GetLobbyGameMode();
	if (!GameMode || !GameMode->HasAuthority())
	{
		return;
	}

	if (!IsLobbyReadyForSelectedMap())
	{
		if (GameMode->MatchCoordinator)
		{
			GameMode->MatchCoordinator->CancelPendingGameStart(TEXT("pre_start_session_validation"));
		}
		return;
	}

	UOnlineSessionsSubsystem* OnlineSessionsSubsystem = GameMode->GetGameInstance()
		? GameMode->GetGameInstance()->GetSubsystem<UOnlineSessionsSubsystem>()
		: nullptr;
	if (!OnlineSessionsSubsystem)
	{
		StartGameTravel();
		return;
	}

	ClearStartSessionDelegate();
	StartSessionCompleteHandle = OnlineSessionsSubsystem->OnStartSessionComplete.AddUObject(
		this,
		&ThisClass::HandleStartSessionComplete);
	OnlineSessionsSubsystem->StartSession();
}

void ULobbyTravelCoordinator::CancelPendingTravel()
{
	ClearStartSessionDelegate();
	if (ALobbyGameMode* GameMode = GetLobbyGameMode())
	{
		GameMode->GetWorldTimerManager().ClearTimer(
			GameEntryContentPreloadPollTimerHandle);
		GameMode->GetWorldTimerManager().ClearTimer(TravelDelayTimerHandle);
	}
	PendingTravelMapName.Reset();
}

void ULobbyTravelCoordinator::Shutdown()
{
	CancelPendingTravel();
}

void ULobbyTravelCoordinator::SetAllLobbyPawnsTravelLocked(const bool bLocked) const
{
	const ALobbyGameMode* GameMode = GetLobbyGameMode();
	const UWorld* World = GameMode ? GameMode->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		SetLobbyPawnTravelLocked(Cast<APlayerController>(It->Get()), bLocked);
	}
}

void ULobbyTravelCoordinator::SetLobbyPawnTravelLocked(
	APlayerController* PlayerController,
	const bool bLocked) const
{
	if (!PlayerController)
	{
		return;
	}

	if (ACharacter* Character = Cast<ACharacter>(PlayerController->GetPawn()))
	{
		if (UCharacterMovementComponent* MovementComponent = Character->GetCharacterMovement())
		{
			MovementComponent->StopMovementImmediately();
			MovementComponent->SetBase(nullptr);

			if (bLocked)
			{
				MovementComponent->DisableMovement();
			}
			else
			{
				MovementComponent->SetMovementMode(MOVE_Walking);
			}
		}
	}

	if (ALobbyPlayerController* LobbyPlayerController = Cast<ALobbyPlayerController>(PlayerController))
	{
		LobbyPlayerController->Client_SetLobbyTravelLock(bLocked);
	}
}

ALobbyGameMode* ULobbyTravelCoordinator::GetLobbyGameMode() const
{
	return Cast<ALobbyGameMode>(GetOuter());
}

bool ULobbyTravelCoordinator::IsLobbyReadyForSelectedMap() const
{
	const ALobbyGameMode* GameMode = GetLobbyGameMode();
	const ULobbyMatchCoordinator* MatchCoordinator = GameMode
		? GameMode->MatchCoordinator.Get()
		: nullptr;
	if (!GameMode || !GameMode->HasAuthority() || !MatchCoordinator)
	{
		return false;
	}

	const int32 ActivePlayerCount = MatchCoordinator->GetActiveLobbyPlayerCount();
	const int32 MaxPlayerCount = FMath::Max(GameMode->GetConfiguredMaxPlayerCount(), 1);
	return ActivePlayerCount > 0
		&& ActivePlayerCount <= MaxPlayerCount
		&& MatchCoordinator->AreLobbyTeamsBalanced();
}

void ULobbyTravelCoordinator::HandleStartSessionComplete(const bool bWasSuccessful)
{
	ClearStartSessionDelegate();
	if (!bWasSuccessful)
	{
		if (ALobbyGameMode* GameMode = GetLobbyGameMode();
			GameMode && GameMode->MatchCoordinator)
		{
			GameMode->MatchCoordinator->CancelPendingGameStart(
				TEXT("start_online_session_failed"));
		}
		return;
	}

	StartGameTravel();
}

void ULobbyTravelCoordinator::ClearStartSessionDelegate()
{
	ALobbyGameMode* GameMode = GetLobbyGameMode();
	UOnlineSessionsSubsystem* OnlineSessionsSubsystem = GameMode && GameMode->GetGameInstance()
		? GameMode->GetGameInstance()->GetSubsystem<UOnlineSessionsSubsystem>()
		: nullptr;
	if (OnlineSessionsSubsystem && StartSessionCompleteHandle.IsValid())
	{
		OnlineSessionsSubsystem->OnStartSessionComplete.Remove(StartSessionCompleteHandle);
	}

	StartSessionCompleteHandle.Reset();
}

void ULobbyTravelCoordinator::StartGameTravel()
{
	ALobbyGameMode* GameMode = GetLobbyGameMode();
	if (!GameMode || !GameMode->HasAuthority())
	{
		return;
	}
	if (!IsLobbyReadyForSelectedMap())
	{
		if (GameMode->MatchCoordinator)
		{
			GameMode->MatchCoordinator->CancelPendingGameStart(TEXT("pre_travel_map_capacity_validation"));
		}
		return;
	}

	FLobbyMatchMapOption SelectedMapOption;
	FString TravelMapName;
	if (!ResolveSelectedGameTravel(TravelMapName, SelectedMapOption))
	{
		if (GameMode->MatchCoordinator)
		{
			GameMode->MatchCoordinator->CancelPendingGameStart(TEXT("travel_map_missing"));
		}
		return;
	}

	PersistSelectedGameConfig(SelectedMapOption, TravelMapName);
	CacheLobbyTravelState(GameMode->GetGameInstance<UPdGameInstance>());
	SetAllLobbyPawnsTravelLocked(true);
	ShowGameStartConnectingPopupForAllPlayers();
	ScheduleServerTravelWhenContentReady(
		BuildGameTravelUrl(TravelMapName));
}

bool ULobbyTravelCoordinator::ResolveSelectedGameTravel(
	FString& OutTravelMapName,
	FLobbyMatchMapOption& OutSelectedMapOption) const
{
	OutTravelMapName.Reset();
	OutSelectedMapOption = FLobbyMatchMapOption();

	const ALobbyGameMode* GameMode = GetLobbyGameMode();
	if (!GameMode)
	{
		return false;
	}

	const UPdGameInstance* PdGameInstance = GameMode->GetGameInstance<UPdGameInstance>();
	FName SelectedMapKey = NAME_None;
	if (const ALobbyGameState* LobbyGameState = GameMode->GetGameState<ALobbyGameState>())
	{
		SelectedMapKey = LobbyGameState->GetSelectedMapKey();
	}
	if (SelectedMapKey.IsNone() && PdGameInstance)
	{
		SelectedMapKey = PdGameInstance->GetLobbySelectedMapKey();
	}

	const FName ResolvedMapKey = GameMode->ResolveConfiguredMapKey(SelectedMapKey);
	if (!GameMode->FindConfiguredMapOption(ResolvedMapKey, OutSelectedMapOption))
	{
		return false;
	}

	OutTravelMapName = GameMode->ResolveTravelMapName(OutSelectedMapOption.MapKey);
	return !OutTravelMapName.IsEmpty();
}

FString ULobbyTravelCoordinator::BuildGameTravelUrl(const FString& TravelMapName) const
{
	FString TravelUrl = TravelMapName;
	if (ShouldStartGameWithoutMatchTimer())
	{
		TravelUrl += FString::Printf(TEXT("?%s=1"), LabGameSession::NoMatchTimerOption);
	}
	return TravelUrl;
}

bool ULobbyTravelCoordinator::ShouldStartGameWithoutMatchTimer() const
{
	const ALobbyGameMode* GameMode = GetLobbyGameMode();
	return GameMode
		&& GameMode->MatchCoordinator
		&& GameMode->MatchCoordinator->GetActiveLobbyPlayerCount() == 1;
}

void ULobbyTravelCoordinator::PersistSelectedGameConfig(
	const FLobbyMatchMapOption& SelectedMapOption,
	const FString& TravelMapName) const
{
	ALobbyGameMode* GameMode = GetLobbyGameMode();
	UPdGameInstance* PdGameInstance = GameMode
		? GameMode->GetGameInstance<UPdGameInstance>()
		: nullptr;
	if (!GameMode || !PdGameInstance)
	{
		return;
	}

	FLobbyMatchMapOption RuntimeMapOption = SelectedMapOption;
	RuntimeMapOption.MaxPlayerCount = FMath::Max(RuntimeMapOption.MaxPlayerCount, 1);
	PdGameInstance->SetLobbyGameConfig(
		RuntimeMapOption.MapKey,
		TravelMapName,
		RuntimeMapOption.MaxPlayerCount,
		FMath::Clamp(PdGameInstance->GetLobbyMaxBotCount(), 0, 100));

	if (ALobbyGameState* LobbyGameState = GameMode->GetGameState<ALobbyGameState>())
	{
		LobbyGameState->SetSelectedMapOption(RuntimeMapOption);
	}
}

void ULobbyTravelCoordinator::CacheLobbyTravelState(UPdGameInstance* PdGameInstance) const
{
	const ALobbyGameMode* GameMode = GetLobbyGameMode();
	const AGameStateBase* GameState = GameMode
		? GameMode->GetGameState<AGameStateBase>()
		: nullptr;
	if (!PdGameInstance || !GameState)
	{
		return;
	}

	PdGameInstance->ResetCachedPlayerMatchIdentities();
	PdGameInstance->ResetCachedLobbyEquippedSkinSlots();
	PdGameInstance->ResetCachedLobbyPandoraLoadouts();
	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		if (const ALobbyPlayerState* LobbyPlayerState = Cast<ALobbyPlayerState>(PlayerState))
		{
			CacheLobbyPlayerTravelState(PdGameInstance, LobbyPlayerState);
		}
	}
}

void ULobbyTravelCoordinator::CacheLobbyPlayerTravelState(
	UPdGameInstance* PdGameInstance,
	const ALobbyPlayerState* LobbyPlayerState) const
{
	const ALobbyGameMode* GameMode = GetLobbyGameMode();
	if (!GameMode || !PdGameInstance || !LobbyPlayerState)
	{
		return;
	}

	PdGameInstance->CachePlayerMatchIdentityForPlayerState(
		LobbyPlayerState,
		LobbyPlayerState->GetPlayerMatchComponent()->GetPlayerMatchIdentity());

	const APlayerController* LobbyPlayerController =
		GameMode->ResolvePlayerControllerForPlayerState(LobbyPlayerState);
	PdGameInstance->CacheLobbyEquippedSkinSlotsForPlayerState(
		LobbyPlayerState,
		BuildEquippedSkinNamesBySlot(LobbyPlayerController));

	TMap<EEnum_Direction, FName> PandoraNamesByDirection;
	if (const UPandoraComponent* PandoraComponent = LobbyPlayerState->GetPandoraComponent())
	{
		for (const EEnum_Direction Direction :
			{ EEnum_Direction::Left, EEnum_Direction::Up, EEnum_Direction::Right })
		{
			if (const UPandoraDefinition* PandoraDefinition =
				PandoraComponent->GetPandoraLoadoutDefinition(Direction))
			{
				PandoraNamesByDirection.Add(Direction, PandoraDefinition->GetFName());
			}
		}
	}
	PdGameInstance->CacheLobbyPandoraLoadoutForPlayerState(
		LobbyPlayerState,
		PandoraNamesByDirection);
}

TMap<FGameplayTag, FName> ULobbyTravelCoordinator::BuildEquippedSkinNamesBySlot(
	const APlayerController* PlayerController) const
{
	TMap<FGameplayTag, FName> EquippedSkinNamesBySlot;
	const APdPlayer* LobbyPlayer = PlayerController
		? Cast<APdPlayer>(PlayerController->GetPawn())
		: nullptr;
	const USkinEquipmentComponent* SkinEquipmentComponent =
		LobbyPlayer ? LobbyPlayer->GetSkinEquipmentComponent() : nullptr;
	if (!SkinEquipmentComponent)
	{
		return EquippedSkinNamesBySlot;
	}

	TArray<FEquippedSkinSlot> EquippedSkinSlots;
	SkinEquipmentComponent->GetEquippedSkinSlots(EquippedSkinSlots);
	for (const FEquippedSkinSlot& EquippedSkinSlot : EquippedSkinSlots)
	{
		if (EquippedSkinSlot.SlotTag.IsValid() && EquippedSkinSlot.SkinDefinition)
		{
			EquippedSkinNamesBySlot.Add(
				EquippedSkinSlot.SlotTag,
				EquippedSkinSlot.SkinDefinition->GetFName());
		}
	}

	return EquippedSkinNamesBySlot;
}

void ULobbyTravelCoordinator::ShowGameStartConnectingPopupForAllPlayers() const
{
	const ALobbyGameMode* GameMode = GetLobbyGameMode();
	const UWorld* World = GameMode ? GameMode->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (ALobbyPlayerController* LobbyPlayerController = Cast<ALobbyPlayerController>(It->Get()))
		{
			LobbyPlayerController->Client_ShowGameStartConnectingPopup();
		}
	}
}

void ULobbyTravelCoordinator::HideGameStartConnectingPopupForAllPlayers() const
{
	const ALobbyGameMode* GameMode = GetLobbyGameMode();
	const UWorld* World = GameMode ? GameMode->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	for (FConstPlayerControllerIterator It =
		World->GetPlayerControllerIterator(); It; ++It)
	{
		if (ALobbyPlayerController* LobbyPlayerController =
			Cast<ALobbyPlayerController>(It->Get()))
		{
			LobbyPlayerController->Client_HideGameStartConnectingPopup();
		}
	}
}

void ULobbyTravelCoordinator::ScheduleServerTravelWhenContentReady(
	const FString& TravelMapName)
{
	ALobbyGameMode* GameMode = GetLobbyGameMode();
	UWorld* World = GameMode ? GameMode->GetWorld() : nullptr;
	if (!GameMode || !World || TravelMapName.IsEmpty())
	{
		return;
	}

	PendingTravelMapName = TravelMapName;
	ULobbyRuntimeSubsystem* LobbyRuntimeSubsystem =
		GameMode->GetGameInstance()
			? GameMode->GetGameInstance()->GetSubsystem<ULobbyRuntimeSubsystem>()
			: nullptr;
	if (!LobbyRuntimeSubsystem)
	{
		HandleGameEntryContentPreloadFailure(
			ELobbyContentPreloadResult::Failed);
		return;
	}

	LobbyRuntimeSubsystem->BeginGameEntryContentPreload();
	const ELobbyContentPreloadResult PreloadResult =
		LobbyRuntimeSubsystem->GetGameEntryContentPreloadResult();
	if (PreloadResult == ELobbyContentPreloadResult::Success)
	{
		World->GetTimerManager().ClearTimer(
			GameEntryContentPreloadPollTimerHandle);
		const FString ReadyTravelMapName = MoveTemp(PendingTravelMapName);
		ScheduleServerTravel(ReadyTravelMapName);
		return;
	}
	if (PreloadResult != ELobbyContentPreloadResult::Loading)
	{
		HandleGameEntryContentPreloadFailure(PreloadResult);
		return;
	}

	if (!World->GetTimerManager().IsTimerActive(
		GameEntryContentPreloadPollTimerHandle))
	{
		World->GetTimerManager().SetTimer(
			GameEntryContentPreloadPollTimerHandle,
			this,
			&ThisClass::HandleGameEntryContentPreloadPoll,
			0.05f,
			true);
	}
}

void ULobbyTravelCoordinator::HandleGameEntryContentPreloadPoll()
{
	ALobbyGameMode* GameMode = GetLobbyGameMode();
	UWorld* World = GameMode ? GameMode->GetWorld() : nullptr;
	if (!GameMode || !World || PendingTravelMapName.IsEmpty())
	{
		CancelPendingTravel();
		return;
	}

	const ULobbyRuntimeSubsystem* LobbyRuntimeSubsystem =
		GameMode->GetGameInstance()
			? GameMode->GetGameInstance()->GetSubsystem<ULobbyRuntimeSubsystem>()
			: nullptr;
	if (!LobbyRuntimeSubsystem)
	{
		HandleGameEntryContentPreloadFailure(
			ELobbyContentPreloadResult::Failed);
		return;
	}

	const ELobbyContentPreloadResult PreloadResult =
		LobbyRuntimeSubsystem->GetGameEntryContentPreloadResult();
	if (PreloadResult == ELobbyContentPreloadResult::Loading)
	{
		return;
	}
	if (PreloadResult != ELobbyContentPreloadResult::Success)
	{
		HandleGameEntryContentPreloadFailure(PreloadResult);
		return;
	}

	World->GetTimerManager().ClearTimer(
		GameEntryContentPreloadPollTimerHandle);
	const FString ReadyTravelMapName = MoveTemp(PendingTravelMapName);
	ScheduleServerTravel(ReadyTravelMapName);
}

void ULobbyTravelCoordinator::HandleGameEntryContentPreloadFailure(
	const ELobbyContentPreloadResult Result)
{
	UE_LOG(
		LogLobbyTravelCoordinator,
		Error,
		TEXT("Game travel was canceled because required content preload ended with result '%s'."),
		*UEnum::GetValueAsString(Result));

	ALobbyGameMode* GameMode = GetLobbyGameMode();
	if (!GameMode)
	{
		CancelPendingTravel();
		return;
	}

	if (GameMode->MatchCoordinator)
	{
		GameMode->MatchCoordinator->CancelPendingGameStart(
			TEXT("game_entry_content_preload_failed"));
	}
	else
	{
		CancelPendingTravel();
		SetAllLobbyPawnsTravelLocked(false);
	}
	HideGameStartConnectingPopupForAllPlayers();
}

void ULobbyTravelCoordinator::ScheduleServerTravel(const FString& TravelMapName)
{
	ALobbyGameMode* GameMode = GetLobbyGameMode();
	UWorld* World = GameMode ? GameMode->GetWorld() : nullptr;
	if (!World || TravelMapName.IsEmpty())
	{
		return;
	}

	World->GetTimerManager().ClearTimer(TravelDelayTimerHandle);
	World->GetTimerManager().SetTimer(
		TravelDelayTimerHandle,
		FTimerDelegate::CreateWeakLambda(this, [this, TravelMapName]()
		{
			if (const ALobbyGameMode* GameMode = GetLobbyGameMode())
			{
				if (UWorld* TravelWorld = GameMode->GetWorld())
				{
					TravelWorld->ServerTravel(TravelMapName);
				}
			}
		}),
		0.15f,
		false);
}
