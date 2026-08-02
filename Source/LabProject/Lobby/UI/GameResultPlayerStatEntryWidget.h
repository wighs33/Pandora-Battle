#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/GameResultTypes.h"
#include "GameResultPlayerStatEntryWidget.generated.h"

class UTextBlock;
class UBorder;
class UImage;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UGameResultPlayerStatEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintCallable, Category = "!GameResult")
	void SetInfo(const FGameResultPlayerStat& InPlayerStat, int32 InWinnerTeamColorIndex);

	UFUNCTION(BlueprintCallable, Category = "!GameResult")
	void SetShowReward(bool bInShowReward);

	UFUNCTION(BlueprintCallable, Category = "!GameResult")
	void RefreshUI();

protected:
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!GameResult|Text")
	FText UnknownPlayerText = NSLOCTEXT("GameResult", "UnknownPlayerText", "Unknown");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!GameResult|Text")
	FText UnknownTeamText = NSLOCTEXT("GameResult", "UnknownTeamText", "No Team");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!GameResult|Text")
	FText RewardTextFormat = NSLOCTEXT("GameResult", "RewardTextFormat", "{0}");

private:
	UPROPERTY(Transient)
	FGameResultPlayerStat PlayerStat;

	UPROPERTY(Transient)
	int32 WinnerTeamColorIndex = INDEX_NONE;

	bool bHasPlayerStat = false;
	bool bShowReward = true;
};
