#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GameplayTagContainer.h"
#include "InputActionValue.h"
#include "UI/InfoUiTypes.h"
#include "UI/KillLogTypes.h"
#include "UI/NotificationData.h"
#include "PdHUD.generated.h"

class APdPlayerController;
class UInfoUiPresenter;
class UDamageScreenEffectWidget;
class UGoldenKillAnnouncementWidget;
class UHudTimerWidget;
class UInfoWidget;
class UKillLogWidget;
class UMenuPopupWidget;
class UPdHudMenuLayer;
class UPdHudScreenLayer;
class UPdHudScoreboardLayer;
class UPdHudUiRouter;
class URespawnDelayWidget;
class URightNotificationsWidget;
class USelectPandoraWidget;
class UPandoraTreeWidget;
class UUiSubsystem;
class UUserWidget;
class UWidgetClassDefinition;

UCLASS()
class LABPROJECT_API APdHUD : public AHUD
{
	GENERATED_BODY()

public:
	APdHUD(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void PreInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	void InitializeUi(UWidgetClassDefinition* InWidgetClassDefinition);
	void DeinitializeUi(const UWidgetClassDefinition* InWidgetClassDefinition);

	UFUNCTION(BlueprintCallable, Category = "!UI")
	void CreateAllUi();

	void RefreshHudTimerVisibility();

	void OpenInfoUiFocused(EPdInfoUiSection Section);

	UFUNCTION(BlueprintCallable, Category = "!UI|Info")
	void CloseInfoUi();

	UFUNCTION(BlueprintCallable, Category = "!UI|Info")
	void ToggleInfoUi();

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void OpenPandoraTreeUi();

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void ClosePandoraTreeUi();

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void TogglePandoraTreeUi();

	UFUNCTION(BlueprintCallable, Category = "!UI")
	void ToggleUiMode(bool bOn);

	UFUNCTION()
	virtual bool IsGameplayInputBlockedByUi() const;

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void OpenSelectPandoraUi();

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	bool CloseSelectPandoraUi();

	void UpdateSelectPandoraDirectionFromMouse();
	void ShowAimCrosshair(FGameplayTag DesiredCrosshairWidgetTag);
	void HideAimCrosshair();
	void ShowRightNotification(const FPdNotificationData& NotificationData);
	void ShowDamageScreenEffect(float DamageAmount);
	void ShowGoldenKillAnnouncement(const FText& AnnouncementText = FText::GetEmpty());
	void AddKillLogEntry(const FKillLogEntry& KillLogEntry);
	void ShowRespawnDelay(float DelaySeconds);
	void HideRespawnDelay();
	void ShowInGameScoreboard();
	void HideInGameScoreboard();
	void RefreshInGameScoreboard();

	UFUNCTION(BlueprintCallable, Category = "!UI|Menu")
	void OpenSettingsMenu();

	UFUNCTION(BlueprintCallable, Category = "!UI|Menu")
	void ToggleSettingsMenu();

	UFUNCTION(BlueprintCallable, Category = "!UI|Menu")
	virtual bool HandleEscapeInput();

	void RefreshUiBindings();

	UInfoWidget* GetInfoWidget() const { return CachedInfoUI; }
	USelectPandoraWidget* GetSelectPandoraWidget() const { return CachedSelectPandoraUI; }
	bool IsSelectPandoraUiOpen() const;
	UUserWidget* GetPlayerHudWidget() const { return CachedPlayerHUD; }
	const UWidgetClassDefinition* GetWidgetClassDefinition() const { return WidgetClassDefinition; }

	void OnOpenSettingsMenuInputStarted(const FInputActionValue& InputValue);
	void OnSelectPandoraInputStarted(const FInputActionValue& InputValue);
	bool OnSelectPandoraInputEnded(const FInputActionValue& InputValue);
	void OnPandoraTreeInputStarted(const FInputActionValue& InputValue);

protected:
	/**
	 * Returns whether a full-screen or modal UI currently owns the screen and
	 * should suppress the normal gameplay HUD. Derived HUDs can add their own UI.
	 */
	virtual bool IsPlayerHudSuppressedByUi() const;

	/** Re-evaluates the gameplay HUD from the shared suppression policy. */
	void RefreshPlayerHudVisibility();

private:
	friend class UPdHudMenuLayer;
	friend class UPdHudScreenLayer;
	friend class UPdHudScoreboardLayer;
	friend class UPdHudUiRouter;

	APdPlayerController* GetPdController() const;
	UPdHudUiRouter* EnsureUiRouter();
	UInfoUiPresenter* GetInfoUiPresenter();
	UUiSubsystem* GetUiSubsystem() const;
	bool ApplyStatusViewModelToWidget(UUserWidget* InWidget);
	bool ApplyStatusViewModelToWidgetTree(UUserWidget* RootWidget);
	bool ApplyStatusViewModelToPlayerHud();
	void ApplyStatusViewModelToPlayerHudRecursive(UUserWidget* RootWidget, bool& bFoundPlayerVitals, bool& bAppliedViewModel);
	UDamageScreenEffectWidget* FindDamageScreenEffectWidget();
	UGoldenKillAnnouncementWidget* FindGoldenKillAnnouncementWidget();
	UKillLogWidget* FindKillLogWidget();
	UHudTimerWidget* FindHudTimerWidget();
	URespawnDelayWidget* FindRespawnDelayWidget();
	bool IsTrainingRoomMap() const;
	void RefreshTrainingRoomUiPause(const UUserWidget* IgnoredWidget = nullptr);
	bool ShouldSuppressHudTimer();
	void ApplyHudTimerVisibility();
	void RetryApplyStatusViewModelToPlayerHud();
	void HandleSettingsMenuLayerClosed();
	void CloseActiveSettingsMenuPopup();
	UMenuPopupWidget* GetActiveSettingsMenuWidget() const;
	bool CloseSelectPandoraUiInternal(bool bCommitSelection);
	void RestoreInfoUiInputMode();
	void ClosePandoraTreeUiInternal(bool bSuppressCameraReturn, bool bImmediate = false);
	void CloseInfoUiInternal(bool bSuppressCameraReturn, bool bImmediate = false);
	void ApplyInventoryWidgetSettings();
	void RemoveAllUiWidgets();

	UPROPERTY(Transient)
	TObjectPtr<UWidgetClassDefinition> WidgetClassDefinition = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UPdHudUiRouter> UiRouter = nullptr;

	UPROPERTY(Transient)
	int32 CachedDirIndex = -1;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> AimCrosshairWidget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> CachedPlayerHUD = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UDamageScreenEffectWidget> CachedDamageScreenEffectWidget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UGoldenKillAnnouncementWidget> CachedGoldenKillAnnouncementWidget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UKillLogWidget> CachedKillLogWidget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UHudTimerWidget> CachedHudTimerWidget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URespawnDelayWidget> CachedRespawnDelayWidget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URightNotificationsWidget> CachedRightNotificationsUI = nullptr;

	FTimerHandle PlayerHudStatusViewModelRetryTimerHandle;
	UPROPERTY(Transient)
	TObjectPtr<UInfoUiPresenter> CachedInfoUiPresenter = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UInfoWidget> CachedInfoUI = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USelectPandoraWidget> CachedSelectPandoraUI = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UPandoraTreeWidget> CachedPandoraTreeUI = nullptr;

};
