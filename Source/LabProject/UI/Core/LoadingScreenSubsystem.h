#pragma once

#include "Containers/Ticker.h"
#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "LoadingScreenSubsystem.generated.h"

struct FWorldContext;
class APlayerController;
class UConnectingPopupWidget;
class UNetDriver;
class UUiScreen;
class UUiSubsystem;
class UWorld;

/**
 * 로컬 플레이어가 기다리는 작업이 있는 동안 연결 대기 팝업을 띄운다.
 *
 * 콘텐츠 로딩·세션 요청·게임 시작 준비·맵 이동·셰이더 컴파일 상태를 각 소유 시스템에서 읽어 대기 사유로 모으고,
 * 사유가 모두 사라지면 팝업을 닫는다. 맵 이동은 출발 월드를 기억했다가 도착 월드의 화면이 준비되면 끝낸다.
 * 화면 스택과 위젯 정의는 UUiSubsystem에서 읽기만 한다.
 */
UCLASS()
class LABPROJECT_API ULoadingScreenSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;

	// Public API ------------------------------------------------------------------------------------------------------
	bool HasBlockingWait() const { return !ActiveWaitReasons.IsEmpty(); }
	/** Server travel preparation, paired by LobbyTravelCoordinator's existing client RPCs. */
	void SetGameStartPreparationPending(bool bPending);

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	/** Rebuilds only the view of existing work. Never begins a wait. */
	void RefreshLoadingScreen();
	bool TickLoadingWork(float DeltaTime);
	void HandlePreClientTravel(const FString& URL, ETravelType TravelType, bool bSeamless);
	void HandlePreLoadMap(const FWorldContext& Context, const FString& MapName);
	void HandleSeamlessTravelStart(UWorld* World, const FString& URL);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type Type, const FString& Error);
	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type Type, const FString& Error);

	UFUNCTION()
	void HandleConnectingPopupCanceled();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void BeginTravel(UWorld* SourceWorld, const FString& URL);
	void AbortTravel();
	bool IsDestinationPresentationReady() const;
	void ShowConnectingPopup(bool bEnableCancelButton);
	void HideConnectingPopup();
	APlayerController* GetLocalPlayerController() const;

private:
	UPROPERTY(Transient)
	TObjectPtr<UUiSubsystem> UiSubsystem;

	UPROPERTY(Transient)
	TObjectPtr<UConnectingPopupWidget> ActiveConnectingPopupWidget;

	UPROPERTY(Transient)
	TObjectPtr<UUiScreen> ConnectingScreen;

	// A snapshot of work owned by content/session systems, plus the cross-world travel transaction.
	enum class EWaitReason : uint8
	{
		StartupContent, LobbyEntryContent, GameEntryContent, SessionRequest,
		SessionLifecycle, GameStartPreparation, Travel, PipelineCompile
	};
	TSet<EWaitReason> ActiveWaitReasons;
	bool bIsDeinitializing = false;
	bool bTravelPending = false;
	bool bGameStartPreparationPending = false;
	TWeakObjectPtr<UWorld> TravelSourceWorld;
	FString TravelDestinationMap;
	uint64 CancelableSessionRequestId = 0;
	FTSTicker::FDelegateHandle LoadingWorkTickerHandle;
	FDelegateHandle WidgetContentChangedHandle;
	FDelegateHandle PreClientTravelHandle;
	FDelegateHandle PreLoadMapHandle;
	FDelegateHandle SeamlessTravelHandle;
	FDelegateHandle TravelFailureHandle;
	FDelegateHandle NetworkFailureHandle;
};
