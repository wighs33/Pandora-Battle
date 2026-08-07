#pragma once

#include "Components/ActorComponent.h"
#include "Containers/Ticker.h"
#include "CoreMinimal.h"
#include "Definition/Player/PlayerControllerDefinition.h"
#include "TimerManager.h"
#include "ControllerPresentationComponent.generated.h"

class ACharacterBase;
class APawn;
class APdPlayerController;
struct FKillLogEntry;
struct FPdNotificationData;

/**
 * Local-player presentation coordinator for APdPlayerController.
 *
 * HUD ownership stays in APdHUD/UUiSubsystem. This component only coordinates
 * controller-local input mode, camera, travel, respawn, and world health bars.
 */
UCLASS(ClassGroup = (PlayerController))
class LABPROJECT_API UControllerPresentationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UControllerPresentationComponent();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void ApplySettings(const FControllerPresentationSettings& InSettings) { Settings = InSettings; }
	void InitializeLocalPresentation();
	void RefreshAfterPossession(APawn* PossessedPawn, bool bRestoreCachedPaintFaceDecal);
	void Shutdown();

	void ApplyCameraViewPitchClamp() const;
	void ShowRightNotification(const FPdNotificationData& NotificationData) const;
	void AddKillLogEntry(const FKillLogEntry& KillLogEntry) const;
	void ShowGoldenKillAnnouncement(const FText& AnnouncementText) const;
	void StartRespawnDelayCountdown(float DelaySeconds) const;
	void HideRespawnDelayCountdown() const;
	void ResetRespawnedPawnStateAtTransform(const FTransform& RespawnTransform);
	void ShowInGameScoreboard();
	void HideInGameScoreboard();

private:
	APdPlayerController* GetPdController() const;
	const APdPlayerController* GetPdControllerConst() const;

	void RestoreGameplayInputMode() const;
	void RefreshTravelLoadingScreen();
	void ScheduleHideTravelLoadingScreenWhenReady();
	bool TickTravelLoadingScreenReady(float DeltaTime);
	void SetTrainingRoomLoadingPaused(bool bPaused);
	void StartHealthBarVisibilityManagement();
	void StopHealthBarVisibilityManagement();
	void UpdateManagedHealthBarVisibility();
	bool ShouldManageHealthBarForTarget(const ACharacterBase* TargetCharacter) const;
	void ResetRespawnedPawnStateForClientAtTransform(FTransform RespawnTransform);
	UPROPERTY(Transient)
	FControllerPresentationSettings Settings;

	FTimerHandle RespawnTransformResetNextTickTimerHandle;
	FTimerHandle RespawnTransformResetRetryTimerHandle;
	FTimerHandle HealthBarVisibilityManagementTimerHandle;
	FTSTicker::FDelegateHandle TravelLoadingReadyTickerHandle;
	int32 TravelLoadingHideRetryCount = 0;
	bool bAppliedTrainingRoomLoadingPause = false;
};
