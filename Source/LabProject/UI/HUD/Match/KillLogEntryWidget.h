#pragma once

#include "CoreMinimal.h"
#include "Common/KillLogTypes.h"
#include "UI/Common/LocalizedMenuWidget.h"
#include "KillLogEntryWidget.generated.h"

class UTextBlock;

UCLASS()
class LABPROJECT_API UKillLogEntryWidget : public ULocalizedMenuWidget
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativeConstruct() override;

	// Public API ------------------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "!KillLog")
	void SetInfo(const FKillLogEntry& InKillLogEntry);

	UFUNCTION(BlueprintCallable, Category = "!KillLog")
	void RefreshUI();

protected:
	virtual void OnMenuLanguageChanged() override;

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	FText BuildKillMessage() const;
	bool ShouldUseSeparatedNameTextBlocks() const;

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!KillLog|Bind")
	TObjectPtr<UTextBlock> Txt_KillerName = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!KillLog|Bind")
	TObjectPtr<UTextBlock> Txt_VictimName = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!KillLog|Bind")
	TObjectPtr<UTextBlock> Txt_KillMessage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!KillLog|Visual")
	FLinearColor KillerNameColor = FLinearColor(0.95f, 0.25f, 0.2f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!KillLog|Visual")
	FLinearColor VictimNameColor = FLinearColor(0.9f, 0.9f, 0.9f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!KillLog|Visual")
	FLinearColor MessageColor = FLinearColor::White;

private:
	UPROPERTY(Transient)
	FKillLogEntry KillLogEntry;

	bool bHasKillLogEntry = false;
};
