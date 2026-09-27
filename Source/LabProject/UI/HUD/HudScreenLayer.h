#pragma once

#include "CoreMinimal.h"
#include "TimerManager.h"
#include "UObject/Object.h"

#include "HudScreenLayer.generated.h"

class APdHUD;
class UPandoraTreeWidget;
class UHudUiRouter;
class UUiScreen;
class UUserWidget;
class FWidgetContentBundleLease;
enum class EInfoUiSection : uint8;

/** 전체 화면 Info·Pandora 계층과 카메라 복귀 정책, 훈련장 일시정지를 조율한다. */
UCLASS()
class LABPROJECT_API UHudScreenLayer : public UObject
{
	GENERATED_BODY()

private:
	enum class EPendingScreenRequest : uint8
	{
		None,
		Info,
		PandoraTree
	};

public:
	// Public API ------------------------------------------------------------------------------------------------------
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
	bool ShouldSuppressPlayerHud() const;

	void RefreshTrainingRoomPause(const UUserWidget* IgnoredWidget = nullptr);
	void ScheduleTrainingRoomPause(float DelaySeconds);

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	UFUNCTION()
	void HandlePandoraTreeClosed(UPandoraTreeWidget* ClosedWidget);

	void FinishCloseInfo();
	void HandleDelayedTrainingRoomPause();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void ClearInfoCloseTimer();
	void ClearTrainingRoomPauseTimer();
	bool IsTrainingRoomPauseUiOpen(const UUserWidget* IgnoredWidget) const;
	void SetTrainingRoomPaused(bool bPaused);
	bool EnsureInfoContentReady(EPendingScreenRequest Request);
	void ContinuePendingScreenOpen();
	void ReleaseInfoContentIfUnused();
	void ReleaseInfoContent();

private:
	TWeakObjectPtr<APdHUD> OwnerHud;
	TWeakObjectPtr<UHudUiRouter> Router;

	UPROPERTY(Transient)
	TObjectPtr<UUiScreen> InfoScreen;

	FTimerHandle InfoCloseTimerHandle;
	FTimerHandle TrainingRoomPauseTimerHandle;
	TSharedPtr<FWidgetContentBundleLease> InfoContentBundleLease;
	EPendingScreenRequest PendingScreenRequest = EPendingScreenRequest::None;
	EInfoUiSection PendingInfoSection;
	bool bAppliedTrainingRoomPause = false;
	bool bInfoClosing = false;
	bool bPandoraTreeClosing = false;
	bool bScreenHandoffInProgress = false;
};
