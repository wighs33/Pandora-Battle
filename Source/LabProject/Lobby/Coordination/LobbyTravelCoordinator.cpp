#include "Lobby/Coordination/LobbyTravelCoordinator.h"

#include "Component/Lobby/LobbyConfigurationComponent.h"
#include "Character/PdPlayer.h"
#include "Common/Enum_Direction.h"
#include "Common/GameSessionConstants.h"
#include "Component/Pandora/PandoraComponent.h"
#include "Component/Skin/SkinEquipmentComponent.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "Definition/Skin/SkinDefinition.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Lobby/Contents/LobbyGameMode.h"
#include "Lobby/Contents/LobbyGameState.h"
#include "Lobby/Contents/LobbyPlayerController.h"
#include "Mode/PdPlayerState.h"
#include "Lobby/LobbyRuntimeSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Online/OnlineSessionsSubsystem.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LobbyTravelCoordinator)

DEFINE_LOG_CATEGORY_STATIC(LogLobbyTravelCoordinator, Log, All);

// GameMode가 확정한 경기 옵션으로 온라인 세션을 시작하고 완료 콜백에서 전장 이동을 준비한다.
void ULobbyTravelCoordinator::StartSessionAndTravel(bool bSuppressMatchTimer)
{
	ALobbyGameMode* GameMode = GetLobbyGameMode();
	if (!GameMode || !GameMode->HasAuthority()) { return; }
	SetGameStartConnectingPopupVisible(true);
	UOnlineSessionsSubsystem* Sessions = GameMode->GetGameInstance()->GetSubsystem<UOnlineSessionsSubsystem>();
	// 전용 서버는 Steam 방 세션을 광고하지 않는다(접속은 IP·매치메이킹). 세션 시작 없이 바로 전장 이동을 준비한다.
	if (!Sessions || GameMode->GetNetMode() == NM_DedicatedServer)
	{
		PrepareMatchTravel(bSuppressMatchTimer);
		return;
	}
	ClearStartSessionDelegate();
	StartSessionCompleteHandle = Sessions->OnStartSessionComplete.AddUObject(this, &ThisClass::HandleStartSessionComplete, bSuppressMatchTimer);
	Sessions->StartSession();
}

// 온라인 세션 시작 구독을 해제한 뒤 성공하면 전장 준비를 계속하고, 실패하면 카운트다운과 이동 잠금을 되돌린다.
void ULobbyTravelCoordinator::HandleStartSessionComplete(bool bWasSuccessful, bool bSuppressMatchTimer)
{
	ClearStartSessionDelegate();
	if (!bWasSuccessful)
	{
		if (ALobbyGameMode* GameMode = GetLobbyGameMode()) { GameMode->CancelPendingGameStart(); }
		return;
	}
	PrepareMatchTravel(bSuppressMatchTimer);
}

// 세션 시작을 기다리는 동안 바뀐 로비 조건을 재검사하고, 선택 맵과 플레이어 정보를 보존한 뒤 접속 팝업과 콘텐츠 로딩을 시작한다.
void ULobbyTravelCoordinator::PrepareMatchTravel(bool bSuppressMatchTimer)
{
	ALobbyGameMode* GameMode = GetLobbyGameMode();
	if (!GameMode || !GameMode->HasAuthority()) { return; }
	FLobbyMatchMapOption SelectedMapOption;
	FString TravelUrl;
	if (!GameMode->bGameStartRequested || !GameMode->AreMatchStartConditionsMet()
		|| !ResolveSelectedMatchMap(TravelUrl, SelectedMapOption))
	{
		GameMode->CancelPendingGameStart();
		return;
	}
	CacheSelectedMapForTravel(SelectedMapOption);
	PrepareLobbyTravelHandoffs();
	if (bSuppressMatchTimer) { TravelUrl += FString::Printf(TEXT("?%s=1"), LabGameSession::NoMatchTimerOption); }
	PreloadContentAndScheduleTravel(TravelUrl);
}

// 서버 GameState가 확정한 선택 맵에서 이동 경로와 정원 설정을 찾는다.
bool ULobbyTravelCoordinator::ResolveSelectedMatchMap(FString& OutTravelMapName, FLobbyMatchMapOption& OutSelectedMapOption) const
{
	OutTravelMapName.Reset();
	OutSelectedMapOption = FLobbyMatchMapOption();

	const ALobbyGameMode* GameMode = GetLobbyGameMode();
	if (!GameMode)
	{
		return false;
	}

	const ALobbyGameState* State = GameMode->GetGameState<ALobbyGameState>();
	if (!State || !GameMode->GetLobbyConfigurationComponent()->FindConfiguredMapOption(State->GetSelectedMapKey(), OutSelectedMapOption))
	{
		return false;
	}

	OutTravelMapName = OutSelectedMapOption.Map.ToSoftObjectPath().GetLongPackageName();
	return !OutTravelMapName.IsEmpty();
}

// 경기를 마치고 로비로 돌아왔을 때 같은 맵을 다시 고르도록 GameInstance의 로비 서브시스템에 보관한다.
void ULobbyTravelCoordinator::CacheSelectedMapForTravel(const FLobbyMatchMapOption& SelectedMapOption) const
{
	ALobbyGameMode* GameMode = GetLobbyGameMode();
	ULobbyRuntimeSubsystem* LobbySubsystem =
		GameMode ? UGameInstance::GetSubsystem<ULobbyRuntimeSubsystem>(GameMode->GetGameInstance()) : nullptr;
	if (!GameMode || !LobbySubsystem)
	{
		return;
	}

	LobbySubsystem->SetLobbySelectedMapKey(SelectedMapOption.MapKey);
}

// 현재 참가자마다 장착 스킨과 판도라 슬롯을 PlayerState에 적는다. 매치 식별 정보와 함께 심리스 이동의 CopyProperties로 경기에 넘어간다.
void ULobbyTravelCoordinator::PrepareLobbyTravelHandoffs() const
{
	const ALobbyGameMode* GameMode = GetLobbyGameMode();
	const AGameStateBase* GameState = GameMode ? GameMode->GetGameState<AGameStateBase>() : nullptr;
	if (!GameState)
	{
		return;
	}

	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		if (APdPlayerState* LobbyPlayerState = Cast<APdPlayerState>(PlayerState))
		{
			LobbyPlayerState->SetLobbyTravelHandoff(BuildLobbyTravelHandoff(*LobbyPlayerState));
		}
	}
}

// 한 참가자의 장착 스킨과 좌·상·우 판도라 슬롯을 애셋 이름으로 모은다.
FLobbyTravelHandoff ULobbyTravelCoordinator::BuildLobbyTravelHandoff(const APdPlayerState& LobbyPlayerState) const
{
	FLobbyTravelHandoff Handoff;
	const ALobbyGameMode* GameMode = GetLobbyGameMode();
	const APlayerController* LobbyPlayerController = LobbyPlayerState.GetPlayerController();
	if (!LobbyPlayerController && GameMode)
	{
		for (FConstPlayerControllerIterator It = GameMode->GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			if (It->Get() && It->Get()->PlayerState == &LobbyPlayerState) { LobbyPlayerController = It->Get(); break; }
		}
	}
	Handoff.EquippedSkinNamesBySlot = BuildEquippedSkinNamesBySlot(LobbyPlayerController);

	if (const UPandoraComponent* PandoraComponent = LobbyPlayerState.GetPandoraComponent())
	{
		for (const EEnum_Direction Direction : {EEnum_Direction::Left, EEnum_Direction::Up, EEnum_Direction::Right})
		{
			if (const UPandoraDefinition* PandoraDefinition = PandoraComponent->GetPandoraLoadoutDefinition(Direction))
			{
				Handoff.PandoraNamesByDirection.Add(Direction, PandoraDefinition->GetFName());
			}
		}
	}
	return Handoff;
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
	if (!GameMode) { return; }
	if (!GameMode->GetWorld() || TravelUrl.IsEmpty())
	{
		GameMode->CancelPendingGameStart();
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

// 필수 콘텐츠 로딩 중이면 로딩 완료 알림을 기다리고, 성공하면 서버 이동을 예약한다. 실패하면 시작을 취소한다.
void ULobbyTravelCoordinator::CheckContentPreloadAndScheduleTravel()
{
	ALobbyGameMode* GameMode = GetLobbyGameMode();
	UWorld* World = GameMode ? GameMode->GetWorld() : nullptr;
	if (!World || PendingTravelUrl.IsEmpty())
	{
		if (GameMode) { GameMode->CancelPendingGameStart(); }
		else { CancelPendingTravel(); }
		return;
	}

	ULobbyRuntimeSubsystem* LobbySubsystem = UGameInstance::GetSubsystem<ULobbyRuntimeSubsystem>(GameMode->GetGameInstance());
	const ELobbyContentPreloadResult PreloadResult =
		LobbySubsystem ? LobbySubsystem->GetGameEntryContentPreloadResult() : ELobbyContentPreloadResult::Failed;
	switch (PreloadResult)
	{
	case ELobbyContentPreloadResult::Loading:
		if (!GameEntryContentPreloadFinishedHandle.IsValid())
		{
			GameEntryContentPreloadFinishedHandle = LobbySubsystem->OnGameEntryContentPreloadFinished().AddUObject(
				this, &ThisClass::HandleGameEntryContentPreloadFinished);
		}
		return;

	case ELobbyContentPreloadResult::Success: {
		StopWaitingForGameEntryContent();
		const FString ReadyTravelUrl = MoveTemp(PendingTravelUrl);
		ScheduleServerTravel(ReadyTravelUrl);
		return;
	}

	default:
		HandleGameEntryContentPreloadFailure(PreloadResult);
		return;
	}
}

void ULobbyTravelCoordinator::HandleGameEntryContentPreloadFinished()
{
	StopWaitingForGameEntryContent();
	CheckContentPreloadAndScheduleTravel();
}

// 필수 콘텐츠 준비 실패를 기록하고 시작 요청·이동 잠금·접속 팝업을 정리해 로비로 되돌린다.
void ULobbyTravelCoordinator::HandleGameEntryContentPreloadFailure(const ELobbyContentPreloadResult Result)
{
	UE_LOG(LogLobbyTravelCoordinator, Error, TEXT("Game travel was canceled because required content preload ended with result '%s'."),
		*UEnum::GetValueAsString(Result));
	if (ALobbyGameMode* GameMode = GetLobbyGameMode()) { GameMode->CancelPendingGameStart(); }
	else { CancelPendingTravel(); }
}

// 접속 팝업 RPC를 보낼 짧은 여유를 둔 뒤 서버와 접속 중인 플레이어들을 전장으로 이동시킨다. 로비 객체가 사라지면 실행하지 않는다.
void ULobbyTravelCoordinator::ScheduleServerTravel(const FString& TravelUrl)
{
	ALobbyGameMode* GameMode = GetLobbyGameMode();
	UWorld* World = GameMode ? GameMode->GetWorld() : nullptr;
	if (!World || TravelUrl.IsEmpty())
	{
		if (GameMode) { GameMode->CancelPendingGameStart(); }
		return;
	}

	World->GetTimerManager().ClearTimer(TravelDelayTimerHandle);
	World->GetTimerManager().SetTimer(TravelDelayTimerHandle,
		FTimerDelegate::CreateWeakLambda(this, [this, TravelUrl]() {
				ALobbyGameMode* LobbyGameMode = GetLobbyGameMode();
				if (!LobbyGameMode) { return; }
				UWorld* TravelWorld = LobbyGameMode->GetWorld();
				if (!TravelWorld || !LobbyGameMode->bGameStartRequested || !LobbyGameMode->AreMatchStartConditionsMet()
					|| !TravelWorld->ServerTravel(TravelUrl))
				{
					LobbyGameMode->CancelPendingGameStart();
				}
			}),
		0.15f, false);
}

// 세션 구독·이동 타이머·대기 URL을 취소하고 접속 팝업과 모든 Pawn의 이동 잠금을 해제한다.
void ULobbyTravelCoordinator::CancelPendingTravel()
{
	ClearStartSessionDelegate();
	StopWaitingForGameEntryContent();
	if (ALobbyGameMode* GameMode = GetLobbyGameMode(); GameMode && GameMode->GetWorld())
	{
		GameMode->GetWorldTimerManager().ClearTimer(TravelDelayTimerHandle);
	}
	PendingTravelUrl.Reset();
	SetGameStartConnectingPopupVisible(false);
	SetAllLobbyPawnsTravelLocked(false);
}

void ULobbyTravelCoordinator::StopWaitingForGameEntryContent()
{
	ALobbyGameMode* GameMode = GetLobbyGameMode();
	ULobbyRuntimeSubsystem* LobbySubsystem =
		GameMode ? UGameInstance::GetSubsystem<ULobbyRuntimeSubsystem>(GameMode->GetGameInstance()) : nullptr;
	if (LobbySubsystem && GameEntryContentPreloadFinishedHandle.IsValid())
	{
		LobbySubsystem->OnGameEntryContentPreloadFinished().Remove(GameEntryContentPreloadFinishedHandle);
	}
	GameEntryContentPreloadFinishedHandle.Reset();
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
			MovementComponent->SetBase(static_cast<FMovementBaseInterfaceData*>(nullptr));

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
