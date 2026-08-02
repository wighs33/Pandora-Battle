#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SavedGameData/PdSaveGame.h"
#include "RecordEntryWidget.generated.h"

class UTextBlock;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API URecordEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintCallable, Category = "!Record")
	void SetRecord(int32 InDisplayNumber, const FPdMatchRecord& InRecord);

	UFUNCTION(BlueprintCallable, Category = "!Record")
	void RefreshUI();

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Record|Bind")
	TObjectPtr<UTextBlock> Txt_Number;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Record|Bind")
	TObjectPtr<UTextBlock> Txt_Result;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Record|Bind")
	TObjectPtr<UTextBlock> Txt_Kills;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Record|Bind")
	TObjectPtr<UTextBlock> Txt_Deaths;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Record|Bind")
	TObjectPtr<UTextBlock> Txt_Reward;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Record|Text")
	FText WinText = NSLOCTEXT("RecordEntryWidget", "WinText", "WIN");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Record|Text")
	FText LoseText = NSLOCTEXT("RecordEntryWidget", "LoseText", "LOSE");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Record|Text")
	FText RewardTextFormat = NSLOCTEXT("RecordEntryWidget", "RewardTextFormat", "{0}");

private:
	void ResolveWidgets();

	UPROPERTY(Transient)
	FPdMatchRecord Record;

	int32 DisplayNumber = 0;
	bool bHasRecord = false;
};
