#pragma once

#include "CoreMinimal.h"
#include "Common/GameResultTypes.h"
#include "UI/Common/LocalizedMenuWidget.h"
#include "GameResultPlayerStatEntryWidget.generated.h"

class UTextBlock;
class UBorder;
class UImage;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UGameResultPlayerStatEntryWidget : public ULocalizedMenuWidget
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativeConstruct() override;

	// Public API ------------------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "!GameResult")
	void SetInfo(const FGameResultPlayerStat& InPlayerStat, int32 InWinnerTeamColorIndex);

	UFUNCTION(BlueprintCallable, Category = "!GameResult")
	void SetShowReward(bool bInShowReward);

	UFUNCTION(BlueprintCallable, Category = "!GameResult")
	void RefreshUI();

protected:
	virtual void OnMenuLanguageChanged() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!GameResult|Bind")
	TObjectPtr<UTextBlock> Txt_PlayerName;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!GameResult|Bind")
	TObjectPtr<UTextBlock> Txt_TeamName;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!GameResult|Bind")
	TObjectPtr<UTextBlock> Txt_Kills;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!GameResult|Bind")
	TObjectPtr<UTextBlock> Txt_Deaths;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!GameResult|Bind")
	TObjectPtr<UTextBlock> Txt_Reward;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!GameResult|Bind")
	TObjectPtr<UImage> Img_Gold;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!GameResult|Bind")
	TObjectPtr<UBorder> ColorBorder;

private:
	UPROPERTY(Transient)
	FGameResultPlayerStat PlayerStat;

	UPROPERTY(Transient)
	int32 WinnerTeamColorIndex = INDEX_NONE;

	bool bHasPlayerStat = false;
	bool bShowReward = true;
};
