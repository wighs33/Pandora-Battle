#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Containers/Ticker.h"
#include "Definition/Player/PlayerControllerDefinition.h"
#include "TimerManager.h"
#include "ControllerPresentationComponent.generated.h"

class ACharacterBase;
class APawn;
class APdPlayerController;
struct FKillLogEntry;
struct FNotificationData;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnRightNotificationRequested, const FNotificationData& /*NotificationData*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnKillLogEntryRequested, const FKillLogEntry& /*KillLogEntry*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnGoldenKillAnnouncementRequested, const FText& /*AnnouncementText*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnRespawnDelayChanged, bool /*bVisible*/, float /*DelaySeconds*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnInGameScoreboardChanged, bool /*bVisible*/);

/**
 * 소유 플레이어의 입력 모드·카메라·로딩 화면·체력바 표시를 조율한다.
 *
 * 위젯 소유권은 HUD와 UI 서브시스템에 두고, HUD에 그릴 내용은 알림으로만 보낸다.
 * 캐릭터의 위치나 사망 상태를 직접 변경하지 않는다.
 */
UCLASS(ClassGroup = (PlayerController))
class LABPROJECT_API UControllerPresentationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Public API ------------------------------------------------------------------------------------------------------
	UControllerPresentationComponent();

	void ApplySettings(const FControllerPresentationSettings& InSettings);
	void InitializeLocalPresentation();
	void RefreshAfterPossession(APawn* PossessedPawn);

	void ShowRightNotification(const FNotificationData& NotificationData) const;
	void AddKillLogEntry(const FKillLogEntry& KillLogEntry) const;
	void ShowGoldenKillAnnouncement(const FText& AnnouncementText) const;
	void StartRespawnDelayCountdown(float DelaySeconds) const;
	void HideRespawnDelayCountdown() const;
	void RefreshAfterRespawn(APawn* RespawnedPawn, const FRotator& RespawnRotation);
	void ShowInGameScoreboard();
	void HideInGameScoreboard();

	/** 로딩 화면이 갱신될 때마다 대기 중인지 알려 준다. 빙의 직후의 훈련실 일시정지가 이 값을 따른다. */
	void SetLoadingScreenWaiting(bool bWaiting) { LoadingScreenWaiting = bWaiting; }

	// HUD가 구독해 그리는 화면 요청. 로컬 HUD가 없으면 구독자도 없어 요청은 버려진다.
	FOnRightNotificationRequested& OnRightNotificationRequested() { return RightNotificationRequested; }
	FOnKillLogEntryRequested& OnKillLogEntryRequested() { return KillLogEntryRequested; }
	FOnGoldenKillAnnouncementRequested& OnGoldenKillAnnouncementRequested() { return GoldenKillAnnouncementRequested; }
	FOnRespawnDelayChanged& OnRespawnDelayChanged() { return RespawnDelayChanged; }
	FOnInGameScoreboardChanged& OnInGameScoreboardChanged() { return InGameScoreboardChanged; }

private:
	void ApplyCameraViewPitchClamp() const;
	// Event Handlers --------------------------------------------------------------------------------------------------
	bool TickTravelLoadingScreenReady(float DeltaTime);
	void UpdateManagedHealthBarVisibility();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	APdPlayerController* GetPdController() const;

	void UpdateTravelLoadingReadyTicker();
	void SetTrainingRoomLoadingPaused(bool bPaused);
	void StartHealthBarVisibilityManagement();
	bool ShouldManageHealthBarForTarget(const ACharacterBase* TargetCharacter) const;

private:
	UPROPERTY(Transient)
	FControllerPresentationSettings Settings;

	FTimerHandle HealthBarVisibilityManagementTimerHandle;
	FTSTicker::FDelegateHandle TravelLoadingReadyTickerHandle;
	TOptional<bool> LoadingScreenWaiting;
	bool bAppliedTrainingRoomLoadingPause = false;

	FOnRightNotificationRequested RightNotificationRequested;
	FOnKillLogEntryRequested KillLogEntryRequested;
	FOnGoldenKillAnnouncementRequested GoldenKillAnnouncementRequested;
	FOnRespawnDelayChanged RespawnDelayChanged;
	FOnInGameScoreboardChanged InGameScoreboardChanged;
};
