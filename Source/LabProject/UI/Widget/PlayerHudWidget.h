#pragma once

#include "CoreMinimal.h"
#include "UI/Widget/LocalizedMenuWidget.h"
#include "TimerManager.h"
#include "PlayerHudWidget.generated.h"

class UHorizontalBox;
class UImage;
class UKillBoxWidget;
class UUserWidget;
class UWidget;
class UWidgetClassDefinition;

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

	// Event Handlers --------------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "!UI|PlayerHUD|KillBox")
	void RefreshKillBox();

protected:
	virtual void OnMenuLanguageChanged() override;

private:
	void HandleAchievementAvatarRefreshRetry();
	void HandleSteamAchievementStateChanged();

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
	void StartAchievementAvatarRefreshRetry();
	void ClearAchievementAvatarRefreshRetry();
	void BindSteamAchievementStateChanged();
	void UnbindSteamAchievementStateChanged();
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

	FTimerHandle KillBoxRefreshTimerHandle;
	FTimerHandle AchievementAvatarRefreshTimerHandle;
	FDelegateHandle SteamAchievementStateChangedHandle;
	int32 AchievementAvatarRefreshRetryCount = 0;
};
