#pragma once

#include "CoreMinimal.h"
#include "UI/Common/LocalizedMenuWidget.h"
#include "Profile/PlayerProfileSaveData.h"
#include "RecordEntryWidget.generated.h"

class UTextBlock;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API URecordEntryWidget : public ULocalizedMenuWidget
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativeConstruct() override;

	// Public API ------------------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "!Record")
	void SetRecord(int32 InDisplayNumber, const FMatchRecord& InRecord);

	UFUNCTION(BlueprintCallable, Category = "!Record")
	void RefreshUI();

protected:
	// Event Handlers --------------------------------------------------------------------------------------------------
	virtual void OnMenuLanguageChanged() override { RefreshUI(); }

private:

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

	/** 승리 글자 색(밝은 금 #F2D99A). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Record|Style")
	FLinearColor WinResultColor = FLinearColor(0.888f, 0.693f, 0.323f);

	/** 패배 글자 색(체력 막대와 같은 빨강 #D9493E). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Record|Style")
	FLinearColor LoseResultColor = FLinearColor(0.693f, 0.066f, 0.048f);

private:
	UPROPERTY(Transient)
	FMatchRecord Record;

	int32 DisplayNumber = 0;
	bool bHasRecord = false;
};
