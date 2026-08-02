#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HudTimerWidget.generated.h"

class UTextBlock;
class UMatchRuleDefinition;
struct FStreamableHandle;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHudTimerFinished);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHudTimerForceMoveTimeReached, const FTransform&, TargetTransform);

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

	// Kept as a no-op so existing widget blueprints continue to load. Match flow is server-owned.
	UFUNCTION(BlueprintCallable, Category = "!Timer|Legacy",
		meta = (DeprecatedFunction, DeprecationMessage = "HUD timers are display-only; force movement is handled by the authoritative GameMode."))
	void ForceMoveOwningPawnNow();

	UFUNCTION(BlueprintCallable, Category = "!Timer")
	void RefreshUI();

	UFUNCTION(BlueprintPure, Category = "!Timer")
	float GetCurrentTimerSeconds() const { return CurrentTimerSeconds; }

	UFUNCTION(BlueprintPure, Category = "!Timer")
	bool IsTimerRunning() const;

	const UMatchRuleDefinition* GetMatchRuleDefinition() const;
	bool ShouldSuppressTimer() const;
	bool ShouldSuppressTimerForCurrentMap() const;

	// Legacy presentation bindings are retained for asset compatibility but are never emitted by the C++ timer.
	UPROPERTY(BlueprintAssignable, Category = "!Timer|Legacy")
	FOnHudTimerFinished OnTimerFinished;

	UPROPERTY(BlueprintAssignable, Category = "!Timer|Legacy")
	FOnHudTimerForceMoveTimeReached OnForceMoveTimeReached;

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

	UFUNCTION(BlueprintImplementableEvent, Category = "!Timer|Legacy", meta = (DisplayName = "On Timer Finished"))
	void BP_OnTimerFinished();

	UFUNCTION(BlueprintImplementableEvent, Category = "!Timer|Legacy", meta = (DisplayName = "On Force Move Time Reached"))
	void BP_OnForceMoveTimeReached(const FTransform& TargetTransform);

private:
	bool BeginMatchRulePreload();
	void ReleaseMatchRulePreload();
	void HandleTimerTick();
	void SyncFromReplicatedTimerState();
	FText FormatTimerText() const;
	float GetConfiguredTimerSeconds() const;
	bool IsCountDownTimer() const;

	FTimerHandle TimerTickHandle;
	int32 MatchRulePreloadGeneration = 0;
	TSharedPtr<FStreamableHandle> MatchRulePreloadHandle;
	float CurrentTimerSeconds = 0.0f;
	bool bRefreshTimerActive = false;
};
