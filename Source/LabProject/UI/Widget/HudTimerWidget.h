#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HudTimerWidget.generated.h"

class UTextBlock;
class UMatchRuleDefinition;
struct FStreamableHandle;

UCLASS()
class LABPROJECT_API UHudTimerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UHudTimerWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintCallable, Category = "!Timer")
	void StartTimer();

	UFUNCTION(BlueprintCallable, Category = "!Timer")
	void PauseTimer();

	UFUNCTION(BlueprintCallable, Category = "!Timer")
	void StopTimer(bool bResetTimer = true);

	UFUNCTION(BlueprintCallable, Category = "!Timer")
	void ResetTimer();

	UFUNCTION(BlueprintCallable, Category = "!Timer")
	void RefreshUI();

	UFUNCTION(BlueprintPure, Category = "!Timer")
	float GetCurrentTimerSeconds() const { return CurrentTimerSeconds; }

	UFUNCTION(BlueprintPure, Category = "!Timer")
	bool IsTimerRunning() const;

	const UMatchRuleDefinition* GetMatchRuleDefinition() const;
	bool ShouldSuppressTimer() const;
	bool ShouldSuppressTimerForCurrentMap() const;

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Timer|Bind")
	TObjectPtr<UTextBlock> Txt_Timer = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Timer|Rules", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UMatchRuleDefinition> MatchRuleDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Timer|Format")
	bool bShowHoursWhenNeeded = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Timer|Format")
	FText FinishedText = NSLOCTEXT("HudTimer", "FinishedText", "00:00");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Timer|Visual")
	FLinearColor NormalTextColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Timer|Visual")
	bool bUseWarningTextColor = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Timer|Visual", meta = (ClampMin = "0.0"))
	float WarningThresholdSeconds = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Timer|Visual")
	FLinearColor WarningTextColor = FLinearColor(1.0f, 0.18f, 0.12f, 1.0f);

private:
	bool BeginMatchRulePreload();
	void ReleaseMatchRulePreload();
	void HandleTimerTick();
	void SyncFromReplicatedTimerState();
	FText FormatTimerText() const;
	float GetConfiguredTimerSeconds() const;

	FTimerHandle TimerTickHandle;
	int32 MatchRulePreloadGeneration = 0;
	TSharedPtr<FStreamableHandle> MatchRulePreloadHandle;
	float CurrentTimerSeconds = 0.0f;
	bool bRefreshTimerActive = false;
};
