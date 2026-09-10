#include "Lobby/Coordination/LobbyTravelCoordinator.h"

#include "Component/Lobby/LobbyPlayerCoordinatorComponent.h"
#include "Component/Lobby/LobbyConfigurationComponent.h"
#include "Character/PdPlayer.h"
#include "Common/Enum_Direction.h"
#include "Common/GameSessionConstants.h"
#include "Component/Pandora/PandoraComponent.h"
#include "Component/Player/PlayerMatchComponent.h"
#include "Component/Skin/SkinEquipmentComponent.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "Definition/Skin/SkinDefinition.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Lobby/Contents/LobbyGameMode.h"
#include "Lobby/Contents/LobbyGameState.h"
#include "Lobby/Contents/LobbyPlayerController.h"
#include "Mode/PdPlayerState.h"
#include "Lobby/Coordination/LobbyMatchCoordinator.h"
#include "Lobby/LobbyRuntimeSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Online/OnlineSessionsSubsystem.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LobbyTravelCoordinator)

DEFINE_LOG_CATEGORY_STATIC(LogLobbyTravelCoordinator, Log, All);

// 카운트다운 종료 시 인원·팀·콘텐츠 준비를 다시 확인하고 온라인 세션을 시작한다. 완료 콜백에서 전장 이동 준비를 이어 간다.
void ULobbyTravelCoordinator::StartSessionAndTravel()
{
	ALobbyGameMode* GameMode = GetLobbyGameMode();
	if (!GameMode || !GameMode->HasAuthority())
	{
		return;
	}

	ULobbyMatchCoordinator* MatchCoordinator = GameMode->GetMatchCoordinator();
	if (!MatchCoordinator)
	{
		return;
	}
	if (!MatchCoordinator->AreMatchStartConditionsMet())
	{
		MatchCoordinator->CancelPendingGameStart();
		return;
	}

	UOnlineSessionsSubsystem* OnlineSessionsSubsystem = UGameInstance::GetSubsystem<UOnlineSessionsSubsystem>(GameMode->GetGameInstance());
	if (!OnlineSessionsSubsystem)
	{
		PrepareMatchTravel();
		return;
	}

	ClearStartSessionDelegate();
	StartSessionCompleteHandle = OnlineSessionsSubsystem->OnStartSessionComplete.AddUObject(this, &ThisClass::HandleStartSessionComplete);
	OnlineSessionsSubsystem->StartSession();
}

// 온라인 세션 시작 구독을 해제한 뒤 성공하면 전장 준비를 계속하고, 실패하면 카운트다운과 이동 잠금을 되돌린다.
void ULobbyTravelCoordinator::HandleStartSessionComplete(const bool bWasSuccessful)
{
	ClearStartSessionDelegate();
	if (!bWasSuccessful)
	{
		if (ALobbyGameMode* GameMode = GetLobbyGameMode(); GameMode && GameMode->GetMatchCoordinator())
		{
			GameMode->GetMatchCoordinator()->CancelPendingGameStart();
		}
		return;
	}

	PrepareMatchTravel();
}

// 세션 시작을 기다리는 동안 바뀐 로비 조건을 재검사하고, 선택 맵과 플레이어 정보를 보존한 뒤 접속 팝업과 콘텐츠 로딩을 시작한다.
void ULobbyTravelCoordinator::PrepareMatchTravel()
{
	ALobbyGameMode* GameMode = GetLobbyGameMode();
	if (!GameMode || !GameMode->HasAuthority())
	{
		return;
	}

	ULobbyMatchCoordinator* MatchCoordinator = GameMode->GetMatchCoordinator();
	if (!MatchCoordinator)
	{
		return;
	}
	if (!MatchCoordinator->AreMatchStartConditionsMet())
	{
		MatchCoordinator->CancelPendingGameStart();
		return;
	}

	FLobbyMatchMapOption SelectedMapOption;
	FString TravelMapName;
	if (!ResolveSelectedMatchMap(TravelMapName, SelectedMapOption))
	{
		MatchCoordinator->CancelPendingGameStart();
		return;
	}

	PersistSelectedGameConfig(SelectedMapOption, TravelMapName);
	CacheLobbyTravelState(UGameInstance::GetSubsystem<ULobbyRuntimeSubsystem>(GameMode->GetGameInstance()));
	SetAllLobbyPawnsTravelLocked(true);
	SetGameStartConnectingPopupVisible(true);
	PreloadContentAndScheduleTravel(BuildGameTravelUrl(TravelMapName));
}

// GameState의 선택 맵을 우선하고 저장된 로비 설정을 보완값으로 사용해, 실제 이동할 맵 경로와 정원 설정을 찾는다.
bool ULobbyTravelCoordinator::ResolveSelectedMatchMap(FString& OutTravelMapName, FLobbyMatchMapOption& OutSelectedMapOption) const
{
	OutTravelMapName.Reset();
	OutSelectedMapOption = FLobbyMatchMapOption();

	const ALobbyGameMode* GameMode = GetLobbyGameMode();
	if (!GameMode)
	{
		return false;
	}

	const ULobbyRuntimeSubsystem* LobbySubsystem = UGameInstance::GetSubsystem<ULobbyRuntimeSubsystem>(GameMode->GetGameInstance());
	FName SelectedMapKey = NAME_None;
	if (const ALobbyGameState* LobbyGameState = GameMode->GetGameState<ALobbyGameState>())
	{
		SelectedMapKey = LobbyGameState->GetSelectedMapKey();
	}
	if (SelectedMapKey.IsNone() && LobbySubsystem)
	{
		SelectedMapKey = LobbySubsystem->GetLobbySelectedMapKey();
	}

	const FName ResolvedMapKey = GameMode->GetLobbyConfigurationComponent()->ResolveConfiguredMapKey(SelectedMapKey);
	if (!GameMode->GetLobbyConfigurationComponent()->FindConfiguredMapOption(ResolvedMapKey, OutSelectedMapOption))
	{
		return false;
	}

	OutTravelMapName = GameMode->GetLobbyConfigurationComponent()->ResolveTravelMapName(OutSelectedMapOption.MapKey);
	return !OutTravelMapName.IsEmpty();
}

// 선택 맵으로 이동할 URL을 만든다. 혼자 입장하면 경기 시간 제한을 끄는 옵션을 붙인다.
FString ULobbyTravelCoordinator::BuildGameTravelUrl(const FString& TravelMapName) const
{
	FString TravelUrl = TravelMapName;
	const ALobbyGameMode* GameMode = GetLobbyGameMode();
	const ULobbyMatchCoordinator* MatchCoordinator = GameMode ? GameMode->GetMatchCoordinator() : nullptr;
	if (MatchCoordinator && MatchCoordinator->GetActiveLobbyPlayerCount() == 1)
	{
		TravelUrl += FString::Printf(TEXT("?%s=1"), LabGameSession::NoMatchTimerOption);
	}
	return TravelUrl;
}

// 맵 이동 뒤에도 선택 맵·정원·봇 설정을 복구할 수 있도록 GameInstance의 로비 서브시스템에 저장하고 GameState에도 반영한다.
void ULobbyTravelCoordinator::PersistSelectedGameConfig(const FLobbyMatchMapOption& SelectedMapOption, const FString& TravelMapName) const
{
	ALobbyGameMode* GameMode = GetLobbyGameMode();
	ULobbyRuntimeSubsystem* LobbySubsystem =
		GameMode ? UGameInstance::GetSubsystem<ULobbyRuntimeSubsystem>(GameMode->GetGameInstance()) : nullptr;
	if (!GameMode || !LobbySubsystem)
	{
		return;
	}

	FLobbyMatchMapOption RuntimeMapOption = SelectedMapOption;
	RuntimeMapOption.MaxPlayerCount = FMath::Max(RuntimeMapOption.MaxPlayerCount, 1);
	LobbySubsystem->SetLobbyGameConfig(RuntimeMapOption.MapKey, TravelMapName, RuntimeMapOption.MaxPlayerCount,
		FMath::Clamp(LobbySubsystem->GetLobbyMaxBotCount(), 0, 100));

	if (ALobbyGameState* LobbyGameState = GameMode->GetGameState<ALobbyGameState>())
	{
		LobbyGameState->SetSelectedMapOption(RuntimeMapOption);
	}
}

// 이전 경기에서 남은 캐시를 비운 후, 현재 참가자들의 매치 식별 정보·스킨·판도라 슬롯을 맵 이동용으로 보관한다.
void ULobbyTravelCoordinator::CacheLobbyTravelState(ULobbyRuntimeSubsystem* LobbySubsystem) const
{
	const ALobbyGameMode* GameMode = GetLobbyGameMode();
	const AGameStateBase* GameState = GameMode ? GameMode->GetGameState<AGameStateBase>() : nullptr;
	if (!LobbySubsystem || !GameState)
	{
		return;
	}

	LobbySubsystem->ResetCachedPlayerMatchIdentities();
	LobbySubsystem->ResetCachedLobbyEquippedSkinSlots();
	LobbySubsystem->ResetCachedLobbyPandoraLoadouts();
	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		if (const APdPlayerState* LobbyPlayerState = Cast<APdPlayerState>(PlayerState))
		{
			CacheLobbyPlayerTravelState(LobbySubsystem, LobbyPlayerState);
		}
	}
}

// 한 참가자의 매치 식별 정보와 장착 스킨, 좌·상·우 판도라 슬롯을 PlayerState 기준으로 로비 서브시스템에 저장한다.
void ULobbyTravelCoordinator::CacheLobbyPlayerTravelState(
	ULobbyRuntimeSubsystem* LobbySubsystem, const APdPlayerState* LobbyPlayerState) const
{
	const ALobbyGameMode* GameMode = GetLobbyGameMode();
	if (!GameMode || !LobbySubsystem || !LobbyPlayerState)
	{
		return;
	}

	LobbySubsystem->CachePlayerMatchIdentityForPlayerState(
		LobbyPlayerState, LobbyPlayerState->GetPlayerMatchComponent()->GetPlayerMatchIdentity());

	const APlayerController* LobbyPlayerController =
		GameMode->GetLobbyPlayerCoordinatorComponent()->ResolvePlayerControllerForPlayerState(LobbyPlayerState);
	LobbySubsystem->CacheLobbyEquippedSkinSlotsForPlayerState(LobbyPlayerState, BuildEquippedSkinNamesBySlot(LobbyPlayerController));

	TMap<EEnum_Direction, FName> PandoraNamesByDirection;
	if (const UPandoraComponent* PandoraComponent = LobbyPlayerState->GetPandoraComponent())
	{
		for (const EEnum_Direction Direction : {EEnum_Direction::Left, EEnum_Direction::Up, EEnum_Direction::Right})
		{
			if (const UPandoraDefinition* PandoraDefinition = PandoraComponent->GetPandoraLoadoutDefinition(Direction))
			{
				PandoraNamesByDirection.Add(Direction, PandoraDefinition->GetFName());
			}
		}
	}
	LobbySubsystem->CacheLobbyPandoraLoadoutForPlayerState(LobbyPlayerState, PandoraNamesByDirection);
}

// 로비 Pawn의 장착 스킨을 슬롯 태그와 애셋 이름으로 변환해, 새 맵에서 같은 외형을 복구할 수 있게 한다.
TMap<FGameplayTag, FName> ULobbyTravelCoordinator::BuildEquippedSkinNamesBySlot(const APlayerController* PlayerController) const
{
	TMap<FGameplayTag, FName> EquippedSkinNamesBySlot;
	const APdPlayer* LobbyPlayer = PlayerController ? Cast<APdPlayer>(PlayerController->GetPawn()) : nullptr;
	const USkinEquipmentComponent* SkinEquipmentComponent = LobbyPlayer ? LobbyPlayer->GetSkinEquipmentComponent() : nullptr;
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
			EquippedSkinNamesBySlot.Add(EquippedSkinSlot.SlotTag, EquippedSkinSlot.SkinDefinition->GetFName());
		}
	}

	return EquippedSkinNamesBySlot;
}

// 전장 맵과 옵션이 담긴 URL을 보관하고 필수 콘텐츠 로딩을 요청한다. 즉시 완료와 비동기 완료를 같은 결과 처리로 보낸다.
void ULobbyTravelCoordinator::PreloadContentAndScheduleTravel(const FString& TravelUrl)
{
	ALobbyGameMode* GameMode = GetLobbyGameMode();
	if (!GameMode || !GameMode->GetWorld() || TravelUrl.IsEmpty())
	{
		return;
	}

	PendingTravelUrl = TravelUrl;
	ULobbyRuntimeSubsystem* LobbySubsystem = UGameInstance::GetSubsystem<ULobbyRuntimeSubsystem>(GameMode->GetGameInstance());
	if (LobbySubsystem)
	{
		LobbySubsystem->BeginGameEntryContentPreload();
	}
	CheckContentPreloadAndScheduleTravel();
}

// 필수 콘텐츠 로딩 중에는 0.05초마다 확인하고, 성공하면 확인 타이머를 해제해 서버 이동을 예약한다. 실패하면 시작을 취소한다.
void ULobbyTravelCoordinator::CheckContentPreloadAndScheduleTravel()
{
	ALobbyGameMode* GameMode = GetLobbyGameMode();
	UWorld* World = GameMode ? GameMode->GetWorld() : nullptr;
	if (!World || PendingTravelUrl.IsEmpty())
	{
		CancelPendingTravel();
		return;
	}

	const ULobbyRuntimeSubsystem* LobbySubsystem = UGameInstance::GetSubsystem<ULobbyRuntimeSubsystem>(GameMode->GetGameInstance());
	const ELobbyContentPreloadResult PreloadResult =
		LobbySubsystem ? LobbySubsystem->GetGameEntryContentPreloadResult() : ELobbyContentPreloadResult::Failed;
	switch (PreloadResult)
	{
	case ELobbyContentPreloadResult::Loading:
		if (!World->GetTimerManager().IsTimerActive(GameEntryContentPreloadPollTimerHandle))
		{
			World->GetTimerManager().SetTimer(
				GameEntryContentPreloadPollTimerHandle, this, &ThisClass::CheckContentPreloadAndScheduleTravel, 0.05f, true);
		}
		return;

	case ELobbyContentPreloadResult::Success: {
		World->GetTimerManager().ClearTimer(GameEntryContentPreloadPollTimerHandle);
		const FString ReadyTravelUrl = MoveTemp(PendingTravelUrl);
		ScheduleServerTravel(ReadyTravelUrl);
		return;
	}

	default:
		HandleGameEntryContentPreloadFailure(PreloadResult);
		return;
	}
}

// 필수 콘텐츠 준비 실패를 기록하고 시작 요청·이동 잠금·접속 팝업을 정리해 로비로 되돌린다.
void ULobbyTravelCoordinator::HandleGameEntryContentPreloadFailure(const ELobbyContentPreloadResult Result)
{
	UE_LOG(LogLobbyTravelCoordinator, Error, TEXT("Game travel was canceled because required content preload ended with result '%s'."),
		*UEnum::GetValueAsString(Result));

	ALobbyGameMode* GameMode = GetLobbyGameMode();
	if (!GameMode)
	{
		CancelPendingTravel();
		return;
	}

	if (GameMode->GetMatchCoordinator())
	{
		GameMode->GetMatchCoordinator()->CancelPendingGameStart();
	}
	else
	{
		CancelPendingTravel();
		SetAllLobbyPawnsTravelLocked(false);
	}
	SetGameStartConnectingPopupVisible(false);
}

// 접속 팝업 RPC를 보낼 짧은 여유를 둔 뒤 서버와 접속 중인 플레이어들을 전장으로 이동시킨다. 로비 객체가 사라지면 실행하지 않는다.
void ULobbyTravelCoordinator::ScheduleServerTravel(const FString& TravelUrl)
{
	ALobbyGameMode* GameMode = GetLobbyGameMode();
	UWorld* World = GameMode ? GameMode->GetWorld() : nullptr;
	if (!World || TravelUrl.IsEmpty())
	{
		return;
	}

	World->GetTimerManager().ClearTimer(TravelDelayTimerHandle);
	World->GetTimerManager().SetTimer(TravelDelayTimerHandle,
		FTimerDelegate::CreateWeakLambda(this,
			[this, TravelUrl]() {
				const ALobbyGameMode* LobbyGameMode = GetLobbyGameMode();
				UWorld* TravelWorld = LobbyGameMode ? LobbyGameMode->GetWorld() : nullptr;
				if (TravelWorld)
				{
					TravelWorld->ServerTravel(TravelUrl);
				}
			}),
		0.15f, false);
}

// 취소된 시작 요청이 나중에 맵을 이동시키지 않도록 세션 완료 구독·콘텐츠 확인·이동 타이머와 대기 URL을 해제한다.
void ULobbyTravelCoordinator::CancelPendingTravel()
{
	ClearStartSessionDelegate();
	if (ALobbyGameMode* GameMode = GetLobbyGameMode())
	{
		GameMode->GetWorldTimerManager().ClearTimer(GameEntryContentPreloadPollTimerHandle);
		GameMode->GetWorldTimerManager().ClearTimer(TravelDelayTimerHandle);
	}
	PendingTravelUrl.Reset();
}

// 로비 종료 시 아직 남아 있는 전장 이동 준비를 정리한다.
void ULobbyTravelCoordinator::Shutdown()
{
	CancelPendingTravel();
}

// 온라인 세션의 늦은 완료 알림이 취소되거나 종료된 로비의 이동 절차를 다시 실행하지 않도록 구독을 해제한다.
void ULobbyTravelCoordinator::ClearStartSessionDelegate()
{
	ALobbyGameMode* GameMode = GetLobbyGameMode();
	UOnlineSessionsSubsystem* OnlineSessionsSubsystem =
		GameMode && GameMode->GetGameInstance() ? GameMode->GetGameInstance()->GetSubsystem<UOnlineSessionsSubsystem>() : nullptr;
	if (OnlineSessionsSubsystem && StartSessionCompleteHandle.IsValid())
	{
		OnlineSessionsSubsystem->OnStartSessionComplete.Remove(StartSessionCompleteHandle);
	}

	StartSessionCompleteHandle.Reset();
}

// 경기 시작 준비 또는 취소에 맞춰 현재 로비의 모든 플레이어에게 이동 잠금 상태를 적용한다.
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
		SetLobbyPawnTravelLocked(It->Get(), bLocked);
	}
}

// 서버 Pawn의 움직임과 이동 기반을 정리하고, 소유 클라이언트에도 이동 잠금 상태를 전달한다. 해제 시 걷기로 복구한다.
void ULobbyTravelCoordinator::SetLobbyPawnTravelLocked(APlayerController* PlayerController, const bool bLocked) const
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

// 호스트와 모든 참가자에게 전장 접속 팝업의 표시·해제를 요청한다. 실제 화면 변경은 각 클라이언트 RPC가 처리한다.
void ULobbyTravelCoordinator::SetGameStartConnectingPopupVisible(const bool bVisible) const
{
	const ALobbyGameMode* GameMode = GetLobbyGameMode();
	const UWorld* World = GameMode ? GameMode->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (ALobbyPlayerController* PlayerController = Cast<ALobbyPlayerController>(It->Get()))
		{
			if (bVisible)
			{
				PlayerController->Client_ShowGameStartConnectingPopup();
			}
			else
			{
				PlayerController->Client_HideGameStartConnectingPopup();
			}
		}
	}
}

// 이 코디네이터를 생성한 로비 GameMode를 가져와 맵 이동과 참가자 처리를 수행한다.
ALobbyGameMode* ULobbyTravelCoordinator::GetLobbyGameMode() const
{
	return Cast<ALobbyGameMode>(GetOuter());
}
