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
struct FPdNotificationData;

/**
 * 소유 플레이어의 입력 모드·카메라·로딩 화면·체력바 표시를 조율한다.
 *
 * 위젯 소유권은 HUD와 UI 서브시스템에 두고,
 * 캐릭터의 위치나 사망 상태를 직접 변경하지 않는다.
 */
UCLASS(ClassGroup = (PlayerController))
class LABPROJECT_API UControllerPresentationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UControllerPresentationComponent();

	//------------------------------------------------------------------------------------------------------------------
	//--- Engine Callbacks
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void ApplySettings(const FControllerPresentationSettings& InSettings);
	void InitializeLocalPresentation();
	void RefreshAfterPossession(APawn* PossessedPawn);

	void ApplyCameraViewPitchClamp() const;
	void ShowRightNotification(const FPdNotificationData& NotificationData) const;
	void AddKillLogEntry(const FKillLogEntry& KillLogEntry) const;
	void ShowGoldenKillAnnouncement(const FText& AnnouncementText) const;
	void StartRespawnDelayCountdown(float DelaySeconds) const;
	void HideRespawnDelayCountdown() const;
	void RefreshAfterRespawn(APawn* RespawnedPawn, const FRotator& RespawnRotation);
	void ShowInGameScoreboard();
	void HideInGameScoreboard();

private:
	APdPlayerController* GetPdController() const;
	void Shutdown();

	void RestoreGameplayInputMode() const;
	void RefreshTravelLoadingScreen();
	void ScheduleHideTravelLoadingScreenWhenReady();
	void UpdateTravelLoadingReadyTicker();
	bool TickTravelLoadingScreenReady(float DeltaTime);
	void SetTrainingRoomLoadingPaused(bool bPaused);
	void StartHealthBarVisibilityManagement();
	void StopHealthBarVisibilityManagement();
	void UpdateManagedHealthBarVisibility();
	bool ShouldManageHealthBarForTarget(const ACharacterBase* TargetCharacter) const;

	UPROPERTY(Transient)
	FControllerPresentationSettings Settings;

	FTimerHandle HealthBarVisibilityManagementTimerHandle;
	FTSTicker::FDelegateHandle TravelLoadingReadyTickerHandle;
	int32 TravelLoadingHideRetryCount = 0;
	bool bAppliedTrainingRoomLoadingPause = false;
};
