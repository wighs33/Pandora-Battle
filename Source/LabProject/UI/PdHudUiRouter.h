#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"

#include "PdHudUiRouter.generated.h"

class APdHUD;
class UGameResultWidget;
class UMenuPopupWidget;
class UUserWidget;
class UWidget;
class UWidgetClassDefinition;

/** Coordinates full-screen Info/Pandora layers, camera-return policy and training-room pause. */
UCLASS()
class LABPROJECT_API UPdHudScreenLayer : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(APdHUD* InOwnerHud, class UPdHudUiRouter* InRouter);
	void Shutdown();

	void OpenInfo();
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
	UFUNCTION()
	void HandlePandoraTreeClosed(class UPandoraTreeWidget* ClosedWidget);

	void FinishCloseInfo();
	void ClearInfoCloseTimer();
	void ClearTrainingRoomPauseTimer();
	void HandleDelayedTrainingRoomPause();
	bool IsTrainingRoomPauseUiOpen(const UUserWidget* IgnoredWidget) const;
	void SetTrainingRoomPaused(bool bPaused);
	void ApplyInfoInputLock();
	void RestoreInfoInputLock();

	TWeakObjectPtr<APdHUD> OwnerHud;
	TWeakObjectPtr<UPdHudUiRouter> Router;

	FTimerHandle InfoCloseTimerHandle;
	FTimerHandle TrainingRoomPauseTimerHandle;
	bool bAppliedTrainingRoomPause = false;
	bool bInfoClosing = false;
	bool bPandoraTreeClosing = false;
	bool bInfoInputLockApplied = false;
	bool bPreviousLookInputIgnored = false;
	bool bPreviousMoveInputIgnored = false;
};

/**
 * Owns the settings-menu layer. APdHUD keeps its public facade, while this
 * object owns widget lifetime and reports close events back to the HUD.
 */
UCLASS()
class LABPROJECT_API UPdHudMenuLayer : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(APdHUD* InOwnerHud, class UPdHudUiRouter* InRouter);
	bool Open();
	bool Toggle();
	bool Close();
	bool IsOpen() const;
	UMenuPopupWidget* GetWidget() const { return ActiveWidget; }
	void Shutdown();

private:
	UFUNCTION()
	void HandleMenuClosed(UMenuPopupWidget* ClosedWidget);

	TWeakObjectPtr<APdHUD> OwnerHud;
	TWeakObjectPtr<UPdHudUiRouter> Router;

	UPROPERTY(Transient)
	TObjectPtr<UMenuPopupWidget> ActiveWidget;
};

/** Owns scoreboard creation, refresh scheduling and stat presentation. */
UCLASS()
class LABPROJECT_API UPdHudScoreboardLayer : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(APdHUD* InOwnerHud, class UPdHudUiRouter* InRouter);
	void Show();
	void Hide();
	void Refresh();
	bool IsOpen() const;
	void Shutdown();

private:
	void BuildPlayerStats(TArray<struct FGameResultPlayerStat>& OutPlayerStats) const;

	TWeakObjectPtr<APdHUD> OwnerHud;
	TWeakObjectPtr<UPdHudUiRouter> Router;

	UPROPERTY(Transient)
	TObjectPtr<UGameResultWidget> ScoreboardWidget;

	FTimerHandle RefreshTimerHandle;
};

/**
 * Per-HUD UI composition router.
 *
 * GameFeature definitions enter through one request stack, core widget layers
 * are created here, and one modal token is used to route HUD input through the
 * LocalPlayer UI subsystem.
 */
UCLASS()
class LABPROJECT_API UPdHudUiRouter : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(APdHUD* InOwnerHud);
	void Shutdown();

	bool AddDefinitionRequest(UWidgetClassDefinition* Definition);
	bool RemoveDefinitionRequest(const UWidgetClassDefinition* Definition);
	UWidgetClassDefinition* GetActiveDefinition() const { return ActiveDefinition; }

	void EnsureCoreLayers();
	void ResetLayers();

	void RouteInput(UWidget* FocusWidget, bool bPreserveGameplayInputMode, bool bCenterCursor);
	void ReleaseInput();

	bool OpenSettingsMenu();
	bool ToggleSettingsMenu();
	bool CloseSettingsMenu();
	bool IsSettingsMenuOpen() const;
	UMenuPopupWidget* GetSettingsMenuWidget() const;

	void ShowScoreboard();
	void HideScoreboard();
	void RefreshScoreboard();
	bool IsScoreboardOpen() const;

	void ShowAimCrosshair(FGameplayTag DesiredCrosshairWidgetTag);
	void HideAimCrosshair();

	void OpenInfo();
	void CloseInfo(bool bSuppressCameraReturn = false, bool bImmediate = false);
	void ToggleInfo();
	void OpenPandoraTree();
	void ClosePandoraTree(bool bSuppressCameraReturn = false, bool bImmediate = false);
	void TogglePandoraTree();
	bool IsInfoClosing() const;
	bool IsPandoraTreeClosing() const;
	bool IsScreenLayerBlockingGameplayInput() const;
	void RefreshTrainingRoomPause(const UUserWidget* IgnoredWidget = nullptr);
	void ScheduleTrainingRoomPause(float DelaySeconds);

private:
	void ApplyActiveDefinition(UWidgetClassDefinition* NewDefinition);
	class UUiSubsystem* ResolveUiSubsystem() const;
	class APdPlayerController* ResolvePlayerController() const;

	TWeakObjectPtr<APdHUD> OwnerHud;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UWidgetClassDefinition>> DefinitionRequests;

	UPROPERTY(Transient)
	TObjectPtr<UWidgetClassDefinition> ActiveDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UPdHudMenuLayer> MenuLayer;

	UPROPERTY(Transient)
	TObjectPtr<UPdHudScreenLayer> ScreenLayer;

	UPROPERTY(Transient)
	TObjectPtr<UPdHudScoreboardLayer> ScoreboardLayer;

	FGuid ModalInputToken;
};
