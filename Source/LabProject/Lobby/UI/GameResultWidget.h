#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/GameResultTypes.h"
#include "GameResultWidget.generated.h"

class UButton;
class UGameResultPlayerStatEntryWidget;
class UPanelWidget;
class UTextBlock;
class UWidget;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UGameResultWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UGameResultWidget(const FObjectInitializer& ObjectInitializer);

	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

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
	void SetShowRewards(bool bInShowRewards);

	UFUNCTION(BlueprintCallable, Category = "!GameResult")
	void RefreshUI();

protected:
	UFUNCTION()
	void HandleExitClicked();

	void HandleEndSessionForExit(bool bWasSuccessful);
	void TravelToLobbyMap();

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!GameResult|Setup")
	TArray<FName> PlayerStatsContainerCandidateNames;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!GameResult|Setup")
	TArray<FName> ScoreboardHiddenWidgetCandidateNames;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!GameResult|Text")
	FText WinnerInfoFormat = NSLOCTEXT("GameResult", "WinnerInfoFormat", "{0}");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!GameResult|Text")
	FText MostKillFormat = NSLOCTEXT("GameResult", "MostKillFormat", "{0} got the most kills ({1})");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!GameResult|Text")
	FText UnknownPlayerText = NSLOCTEXT("GameResult", "UnknownPlayerText", "Unknown");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!GameResult|Travel")
	TSoftObjectPtr<UWorld> LobbyMap;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!GameResult|Travel")
	FString LobbyTravelMapName;

private:
	FString GetResolvedLobbyTravelMapName() const;
	void ResolveExitButton();
	UPanelWidget* FindPlayerStatsContainer();
	void RefreshPlayerStatsList();
	void ApplyDisplayModeVisibility();
	void SetWidgetVisibleForDisplayMode(UWidget* Widget, bool bVisible) const;

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
	bool bShowRewards = true;
};
