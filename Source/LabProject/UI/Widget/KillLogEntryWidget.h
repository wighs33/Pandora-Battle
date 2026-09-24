#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/KillLogTypes.h"
#include "KillLogEntryWidget.generated.h"

class UTextBlock;

UCLASS()
class LABPROJECT_API UKillLogEntryWidget : public UUserWidget
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!KillLog|Text")
	FText KillMessageFormat = NSLOCTEXT("KillLog", "KillMessageFormat", "{0} eliminated {1}");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!KillLog|Text")
	FText KillConnectorText = NSLOCTEXT("KillLog", "KillConnectorText", "eliminated");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!KillLog|Text")
	FText SelfKillMessageFormat = NSLOCTEXT("KillLog", "SelfKillMessageFormat", "{0} eliminated themselves");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!KillLog|Text")
	FText EnvironmentKillMessageFormat = NSLOCTEXT("KillLog", "EnvironmentKillMessageFormat", "{0} died");

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
