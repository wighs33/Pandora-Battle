#pragma once

#include "CoreMinimal.h"
#include "Common/KillLogTypes.h"
#include "Component/Character/AbilitySystemReadySubscription.h"
#include "GameFramework/HUD.h"
#include "GameplayTagContainer.h"
#include "InputActionValue.h"
#include "Interface/HudInputInterface.h"
#include "Common/NotificationData.h"
#include "PdHUD.generated.h"

class ACharacterBase;
class AExperienceGameState;
class AGameStateBase;
class APdPlayerController;
struct FGameResultPlayerStat;
class UInfoUiPresenter;
class UDamageScreenEffectWidget;
class UGoldenKillAnnouncementWidget;
class UHudTimerWidget;
class UInfoWidget;
class UKillLogWidget;
class UHudMenuLayer;
class UHudScreenLayer;
class UHudScoreboardLayer;
class UHudSelectPandoraLayer;
class UHudUiRouter;
class UUiScreen;
class URespawnDelayWidget;
class URightNotificationsWidget;
class USelectPandoraWidget;
class UPandoraTreeWidget;
class UUiSubsystem;
class UUserWidget;
class UWidgetClassDefinition;

UCLASS()
class LABPROJECT_API APdHUD : public AHUD, public IHudInputInterface
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void PreInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	// Public API ------------------------------------------------------------------------------------------------------
	APdHUD(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	void InitializeUi(UWidgetClassDefinition* InWidgetClassDefinition);
	void DeinitializeUi(const UWidgetClassDefinition* InWidgetClassDefinition);

	UFUNCTION(BlueprintCallable, Category = "!UI")
	void CreateAllUi();

	void RefreshHudTimerVisibility();

	virtual void OpenInfoUiFocused(EInfoUiSection Section) override;

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

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void OpenSelectPandoraUi();

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	bool CloseSelectPandoraUi();

	void ShowAimCrosshair(FGameplayTag DesiredCrosshairWidgetTag);
	void HideAimCrosshair();
	void ShowDamageScreenEffect(float DamageAmount);

	UFUNCTION(BlueprintCallable, Category = "!UI|Menu")
	void ToggleEscapeMenu();

	void RefreshUiBindings();

	UInfoWidget* GetInfoWidget() const { return CachedInfoUI; }
	USelectPandoraWidget* GetSelectPandoraWidget() const;
	virtual bool IsSelectPandoraUiOpen() const override;
	virtual bool IsGameplayInputBlockedByUi() const override;
	UUserWidget* GetPlayerHudWidget() const { return CachedPlayerHUD; }
	const UWidgetClassDefinition* GetWidgetClassDefinition() const { return WidgetClassDefinition; }

	// Event Handlers --------------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "!UI|Menu")
	void OpenEscapeMenu();

	UFUNCTION(BlueprintCallable, Category = "!UI|Menu")
	virtual bool HandleEscapeInput() override;

	virtual void OnOpenSettingsMenuInputStarted(const FInputActionValue& InputValue) override;
	virtual void OnSelectPandoraInputStarted(const FInputActionValue& InputValue) override;
	virtual bool OnSelectPandoraInputEnded(const FInputActionValue& InputValue) override;
	virtual void OnPandoraTreeInputStarted(const FInputActionValue& InputValue) override;

private:
	void HandlePossessedCharacterReady(ACharacterBase* Character, UPdAbilitySystemComponent* AbilitySystemComponent);
	void HandleEscapeMenuClosed();
	void HandleAimCrosshairChanged(bool bVisible, FGameplayTag CrosshairWidgetTag);
	void ShowRightNotification(const FPdNotificationData& NotificationData);
	void ShowGoldenKillAnnouncement(const FText& AnnouncementText);
	void AddKillLogEntry(const FKillLogEntry& KillLogEntry);
	void HandleRespawnDelayChanged(bool bVisible, float DelaySeconds);
	void HandleInGameScoreboardChanged(bool bVisible);
	void ShowGameResult(const FText& WinnerTitle, int32 WinnerTeamColorIndex, const FText& MaxKillerName, int32 MaxKillCount,
		const TArray<FGameResultPlayerStat>& PlayerStats);

	// 게임플레이 쪽은 화면을 직접 부르지 않고 컨트롤러·화면 표시 컴포넌트·GameState에 알린다. HUD는 그 알림을 구독해 그린다.
	void BindPresentationEvents();
	void BindGameStateEvents(AGameStateBase* GameState);
	void UnbindPresentationEvents();

protected:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	/**
	 * 전체 화면이나 모달 UI가 화면을 차지해 일반 게임플레이 HUD를 숨겨야 하는지 돌려준다.
	 * 파생 HUD는 자기 UI를 조건에 더할 수 있다.
	 */
	virtual bool IsPlayerHudSuppressedByUi() const;

	/** 공용 숨김 정책으로 게임플레이 HUD 표시 여부를 다시 판단한다. */
	void RefreshPlayerHudVisibility();

private:
	APdPlayerController* GetPdController() const;
	UHudUiRouter* EnsureUiRouter();
	UHudScreenLayer* GetScreenLayer() const;
	UHudMenuLayer* GetMenuLayer() const;
	UHudScoreboardLayer* GetScoreboardLayer() const;
	UHudSelectPandoraLayer* GetSelectPandoraLayer() const;
	bool IsEscapeMenuOpen() const;
	UInfoUiPresenter* GetInfoUiPresenter();
	UUiSubsystem* GetUiSubsystem() const;
	bool ApplyStatusViewModelToWidgetTree(UUserWidget* RootWidget);
	bool ApplyStatusViewModelToPlayerHud();
	UDamageScreenEffectWidget* FindDamageScreenEffectWidget();
	UGoldenKillAnnouncementWidget* FindGoldenKillAnnouncementWidget();
	UKillLogWidget* FindKillLogWidget();
	UHudTimerWidget* FindHudTimerWidget();
	URespawnDelayWidget* FindRespawnDelayWidget();
	bool IsTrainingRoomMap() const;
	void RefreshTrainingRoomUiPause(const UUserWidget* IgnoredWidget = nullptr);
	bool ShouldSuppressHudTimer();
	bool CloseSelectPandoraUiInternal(bool bCommitSelection);
	void ApplyInventoryWidgetSettings();
	void RemoveAllUiWidgets();

private:
	friend class UHudMenuLayer;
	friend class UHudScreenLayer;
	friend class UHudScoreboardLayer;
	friend class UHudSelectPandoraLayer;
	friend class UHudUiRouter;

	UPROPERTY(Transient)
	TObjectPtr<UWidgetClassDefinition> WidgetClassDefinition = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UHudUiRouter> UiRouter = nullptr;

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

	FAbilitySystemReadySubscription PossessedCharacterReadySubscription;

	TWeakObjectPtr<APdPlayerController> PresentationController;
	TWeakObjectPtr<AExperienceGameState> PresentationGameState;

	UPROPERTY(Transient)
	TObjectPtr<UInfoUiPresenter> CachedInfoUiPresenter = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UInfoWidget> CachedInfoUI = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UPandoraTreeWidget> CachedPandoraTreeUI = nullptr;
};
