#pragma once

#include "CoreMinimal.h"
#include "UI/Common/LocalizedMenuWidget.h"
#include "RespawnDelayWidget.generated.h"

class UTextBlock;

UCLASS()
class LABPROJECT_API URespawnDelayWidget : public ULocalizedMenuWidget
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// Public API ------------------------------------------------------------------------------------------------------
	URespawnDelayWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "!Respawn")
	void StartRespawnDelay(float InDelaySeconds);

	UFUNCTION(BlueprintCallable, Category = "!Respawn")
	void HideRespawnDelay();

	UFUNCTION(BlueprintCallable, Category = "!Respawn")
	void RefreshUI();

	UFUNCTION(BlueprintPure, Category = "!Respawn")
	float GetRemainingSeconds() const { return RemainingSeconds; }

	UFUNCTION(BlueprintPure, Category = "!Respawn")
	bool IsRespawnDelayActive() const { return bRespawnDelayActive; }

protected:
	// Event Handlers --------------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintImplementableEvent, Category = "!Respawn", meta = (DisplayName = "On Respawn Delay Started"))
	void BP_OnRespawnDelayStarted(float DelaySeconds);

	UFUNCTION(BlueprintImplementableEvent, Category = "!Respawn", meta = (DisplayName = "On Respawn Delay Finished"))
	void BP_OnRespawnDelayFinished();

	virtual void OnMenuLanguageChanged() override;

private:
	void HandleRespawnDelayTick();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void FinishRespawnDelay();
	FText FormatRespawnDelayText() const;

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Respawn|Bind")
	TObjectPtr<UTextBlock> Txt_RespawnDelay = nullptr;

	/** 번역표(HUD.RespawnIn, HUD.Respawning)에 없을 때 쓰는 문구. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Respawn|Text")
	FText RespawnDelayFormatText = NSLOCTEXT("RespawnDelay", "RespawnDelayFormatText", "Respawn in {Seconds}");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Respawn|Text")
	FText FinishedText = NSLOCTEXT("RespawnDelay", "FinishedText", "Respawning...");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Respawn|Timing", meta = (ClampMin = "0.01"))
	float TickInterval = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Respawn|Timing")
	bool bHideWhenFinished = true;

private:
	FTimerHandle RespawnDelayTickHandle;
	float RemainingSeconds = 0.0f;
	float LastUpdateTimeSeconds = 0.0f;
	bool bRespawnDelayActive = false;
};
