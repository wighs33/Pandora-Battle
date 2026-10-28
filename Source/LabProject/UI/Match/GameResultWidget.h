#pragma once

#include "CoreMinimal.h"
#include "Common/GameResultTypes.h"
#include "UI/Core/PdScreenWidget.h"
#include "GameResultWidget.generated.h"

class UButton;
class UGameResultPlayerStatEntryWidget;
class UPanelWidget;
class UTextBlock;
class UWidget;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UGameResultWidget : public UPdScreenWidget
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// Public API ------------------------------------------------------------------------------------------------------
	UGameResultWidget(const FObjectInitializer& ObjectInitializer);
	void ShowResultScreen();

	UFUNCTION(BlueprintCallable, Category = "!GameResult")
	void SetInfo(
		const FText& InWinnerTitle,
		int32 InWinnerTeamColorIndex,
		const FText& InMaxKillerName,
		int32 InMaxKillCount,
		const TArray<FGameResultPlayerStat>& InPlayerStats);

	UFUNCTION(BlueprintCallable, Category = "!GameResult")
	void SetInGameScoreboardInfo(const TArray<FGameResultPlayerStat>& InPlayerStats);

	UFUNCTION(BlueprintCallable, Category = "!GameResult")
	void SetExitToLobbyEnabled(bool bInExitToLobbyEnabled);

	UFUNCTION(BlueprintCallable, Category = "!GameResult")
	void SetCloseOnlyOnExit(bool bInCloseOnlyOnExit);

	UFUNCTION(BlueprintCallable, Category = "!GameResult")
	void SetShowRewards(bool bInShowRewards);

	UFUNCTION(BlueprintCallable, Category = "!GameResult")
	void SetEndedByPlayerExit(bool bInEndedByPlayerExit);

	UFUNCTION(BlueprintCallable, Category = "!GameResult")
	void RefreshUI();

protected:
	// Event Handlers --------------------------------------------------------------------------------------------------
	virtual void OnMenuLanguageChanged() override;

	UFUNCTION()
	void HandleExitClicked();

	void HandleEndSessionForExit(bool bWasSuccessful);

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void TravelToLobbyMap();

private:
	FString GetResolvedLobbyTravelMapName() const;
	FText MakeTitleText() const;
	bool IsTeamVictoryTitle() const;
	void RefreshPlayerStatsList();
	void ApplyDisplayModeVisibility();
	void SetWidgetVisibleForDisplayMode(UWidget* Widget, bool bVisible) const;

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!GameResult|Bind")
	TObjectPtr<UTextBlock> Txt_WinnerInfo;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!GameResult|Bind")
	TObjectPtr<UTextBlock> Txt_MostKill;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!GameResult|Bind")
	TObjectPtr<UWidget> Txt_Reward;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!GameResult|Bind")
	TObjectPtr<UButton> Btn_Exit;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!GameResult|Bind")
	TObjectPtr<UPanelWidget> PlayerStatsContainer;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!GameResult|Setup")
	TSubclassOf<UGameResultPlayerStatEntryWidget> PlayerStatEntryWidgetClass;

	/** 팀 승리가 아닌 제목(점수판, 나간 플레이어로 끝난 경기)의 색. 팀이 이긴 결과는 팀 색을 쓴다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!GameResult|Style")
	FSlateColor NeutralTitleColor = FSlateColor(FLinearColor(0.846873f, 0.603827f, 0.23074f, 1.0f));

private:
	UPROPERTY(Transient)
	FText WinnerTitle;

	UPROPERTY(Transient)
	int32 WinnerTeamColorIndex = INDEX_NONE;

	UPROPERTY(Transient)
	FText MaxKillerName;

	UPROPERTY(Transient)
	int32 MaxKillCount = 0;

	UPROPERTY(Transient)
	TArray<FGameResultPlayerStat> PlayerStats;

	FDelegateHandle EndSessionCompleteHandle;
	bool bPendingLobbyTravelAfterEndSession = false;
	bool bInGameScoreboardMode = false;
	bool bExitToLobbyEnabled = true;
	bool bCloseOnlyOnExit = false;
	bool bShowRewards = true;
	bool bEndedByPlayerExit = false;
};
