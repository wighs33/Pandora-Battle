#pragma once

#include "CoreMinimal.h"
#include "Online/AchievementNotificationSubscription.h"
#include "UI/Common/LocalizedMenuWidget.h"
#include "TimerManager.h"
#include "PlayerHudWidget.generated.h"

class AWorldBossRaid;
class UHorizontalBox;
class UImage;
class UKillBoxWidget;
class UUserWidget;
class UWidget;
class UWidgetClassDefinition;
class UWorldBossBarWidget;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UPlayerHudWidget : public ULocalizedMenuWidget
{
	GENERATED_BODY()

protected:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

public:
	// Public API ------------------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "!UI|PlayerHUD")
	void InitializePlayerHud(UWidgetClassDefinition* InWidgetClassDefinition);

	UFUNCTION(BlueprintCallable, Category = "!UI|PlayerHUD|KillBox")
	void RebuildKillBox();

	UFUNCTION(BlueprintCallable, Category = "!UI|PlayerHUD|Achievement")
	bool RefreshAchievementAvatar();

	/** 보스 레이드 월드에서 처치 수 자리에 긴 월드 보스 체력바를 놓는다. HUD와 레이드 액터 중 나중에 준비된 쪽이 부른다. */
	void ShowWorldBossBar(AWorldBossRaid& WorldBossRaid);

	// Event Handlers --------------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "!UI|PlayerHUD|KillBox")
	void RefreshKillBox();

protected:
	virtual void OnMenuLanguageChanged() override;

private:
	void HandleAchievementDisplayChanged();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void RefreshLobbyTipVisibility();
	void RefreshKillBoxVisibility();
	bool IsTrainingRoomMap() const;
	TSubclassOf<UKillBoxWidget> ResolveKillBoxWidgetClass() const;
	float ResolveKillBoxRefreshInterval() const;
	FText ResolveTeamName(int32 TeamColorIndex) const;
	bool CanRebuildKillBox() const;
	void CenterKillBoxContainer() const;
	void ResolveActiveTeamStats(TArray<int32>& OutTeamColorIndices, TMap<int32, int32>& OutKillCountByTeam) const;
	bool AreKillBoxWidgetsBuiltForTeams(const TArray<int32>& TeamColorIndices) const;
	void BuildKillBoxWidgetsForTeams(const TArray<int32>& TeamColorIndices);
	void StartKillBoxRefreshTimer();
	void ClearKillBoxWidgets();
	void ClearKillBoxTimer();
	void ClearTransactionalFlagsForRuntimeWidget(UUserWidget* Widget) const;
	UImage* FindImageInUserWidget(UUserWidget* RootWidget, FName ImageName) const;
	UImage* FindImageInWidget(UWidget* RootWidget, FName ImageName) const;

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|PlayerHUD|KillBox")
	TObjectPtr<UHorizontalBox> HorizontalBox_KillBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Txt_LobbyTip;

private:
	UPROPERTY(Transient)
	TObjectPtr<UWidgetClassDefinition> WidgetClassDefinition = nullptr;

	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UKillBoxWidget>> KillBoxWidgets;

	UPROPERTY(Transient)
	TObjectPtr<UWorldBossBarWidget> WorldBossBar;

	FTimerHandle KillBoxRefreshTimerHandle;
	FAchievementNotificationSubscription AchievementSubscription;
};
