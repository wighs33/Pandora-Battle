#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GoldenKillAnnouncementWidget.generated.h"

class UTextBlock;
class UWidgetAnimation;

UCLASS()
class LABPROJECT_API UGoldenKillAnnouncementWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// Public API ------------------------------------------------------------------------------------------------------
	UGoldenKillAnnouncementWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "!GoldenKill")
	void PlayGoldenKillAnnouncement(const FText& OverrideText);

	// Event Handlers --------------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "!GoldenKill")
	void HideGoldenKillAnnouncement();

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	void ApplyTextStyle(const FText& TextToShow);

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!GoldenKill|Bind")
	TObjectPtr<UTextBlock> Txt_GoldenKill = nullptr;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> GoldenKillAnimation = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!GoldenKill|Text")
	FText GoldenKillText = NSLOCTEXT("GoldenKill", "GoldenKillText", "GOLDEN KILL");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!GoldenKill|Text")
	FLinearColor GoldenKillTextColor = FLinearColor(1.0f, 0.75f, 0.08f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!GoldenKill|Timing", meta = (ClampMin = "0.01"))
	float FallbackVisibleDuration = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!GoldenKill|Timing", meta = (ClampMin = "0.01"))
	float AnimationPlaybackSpeed = 1.0f;

private:
	FTimerHandle HideTimerHandle;
};
