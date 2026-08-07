#pragma once

#include "CoreMinimal.h"
#include "TimerManager.h"
#include "UObject/Object.h"

#include "HudScreenLayer.generated.h"

class APdHUD;
class UPandoraTreeWidget;
class UHudUiRouter;
class UUserWidget;
class FWidgetContentBundleLease;
enum class EInfoUiSection : uint8;

/** Coordinates full-screen Info/Pandora layers, camera-return policy and training-room pause. */
UCLASS()
class LABPROJECT_API UHudScreenLayer : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(APdHUD* InOwnerHud, UHudUiRouter* InRouter);
	void Shutdown();

	void OpenInfo();
	void OpenInfo(EInfoUiSection InitialSection);
	void CloseInfo(bool bSuppressCameraReturn = false, bool bImmediate = false);
	void ToggleInfo();

	void OpenPandoraTree();
	void ClosePandoraTree(bool bSuppressCameraReturn = false, bool bImmediate = false);
	void TogglePandoraTree();

	bool IsInfoOpen() const;
	bool IsPandoraTreeOpen() const;
	bool IsInfoClosing() const { return bInfoClosing; }
	bool IsPandoraTreeClosing() const { return bPandoraTreeClosing; }
	bool IsBlockingGameplayInput() const;

	void RefreshTrainingRoomPause(const UUserWidget* IgnoredWidget = nullptr);
	void ScheduleTrainingRoomPause(float DelaySeconds);

private:
	enum class EPendingScreenRequest : uint8
	{
		None,
		Info,
		PandoraTree
	};

	UFUNCTION()
	void HandlePandoraTreeClosed(UPandoraTreeWidget* ClosedWidget);

	void FinishCloseInfo();
	void ClearInfoCloseTimer();
	void ClearTrainingRoomPauseTimer();
	void HandleDelayedTrainingRoomPause();
	bool IsTrainingRoomPauseUiOpen(const UUserWidget* IgnoredWidget) const;
	void SetTrainingRoomPaused(bool bPaused);
	void ApplyInfoInputLock();
	void RestoreInfoInputLock();
	bool EnsureInfoContentReady(EPendingScreenRequest Request);
	void ContinuePendingScreenOpen();
	void ReleaseInfoContentIfUnused();
	void ReleaseInfoContent();

	TWeakObjectPtr<APdHUD> OwnerHud;
	TWeakObjectPtr<UHudUiRouter> Router;

	FTimerHandle InfoCloseTimerHandle;
	FTimerHandle TrainingRoomPauseTimerHandle;
	TSharedPtr<FWidgetContentBundleLease> InfoContentBundleLease;
	EPendingScreenRequest PendingScreenRequest = EPendingScreenRequest::None;
	EInfoUiSection PendingInfoSection;
	bool bAppliedTrainingRoomPause = false;
	bool bInfoClosing = false;
	bool bPandoraTreeClosing = false;
	bool bInfoInputLockApplied = false;
	bool bPreviousLookInputIgnored = false;
	bool bPreviousMoveInputIgnored = false;
	bool bScreenHandoffInProgress = false;
};
