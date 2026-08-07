#pragma once

#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"

#include "StatusEffectWidget.generated.h"

class AActor;
class UAbilitySystemComponent;
class UImage;
class UStatusEffectDefinition;
class UStatusEffectReplicationComponent;
class UProgressBar;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UStatusEffectWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "!UI|StatusEffect")
	void SetOwnerActor(AActor* InOwnerActor);

	UFUNCTION(BlueprintCallable, Category = "!UI|StatusEffect")
	void SetEffectDataAsset(UStatusEffectDefinition* InEffectDataAsset);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "!UI|StatusEffect")
	UStatusEffectDefinition* GetEffectDataAsset() const;

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|StatusEffect|Widgets")
	TObjectPtr<UProgressBar> EffectFillMeter;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|StatusEffect|Widgets")
	TObjectPtr<UImage> EffectIcon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|StatusEffect|Widgets")
	TObjectPtr<UProgressBar> EffectAppliedTimeLeft;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ExposeOnSpawn = "true"), Category = "!UI|StatusEffect")
	TObjectPtr<AActor> OwnerActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ExposeOnSpawn = "true"), Category = "!UI|StatusEffect")
	TObjectPtr<UStatusEffectDefinition> EffectDataAsset;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|StatusEffect")
	int32 CurrentStackCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.001"), Category = "!UI|StatusEffect")
	float MeterUpdateInterval = 0.033333f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"), Category = "!UI|StatusEffect")
	float InitialIconOpacity = 0.65f;

private:
	void ApplyWidgetDefinitionSettings();
	void InitializeStatusEffect();
	void ApplyDesignerDefaults();
	void SetInitialValues();
	void SetIconStyle();
	void RefreshFromActiveEffects();
	void UpdateFillMeter();
	void RestartStackFillPresentation();
	void StartStackFillDecrease();
	void UpdateStackFillDecrease();
	void ClearStackFillPresentationTimers();
	void HandleStatusEffectApplied();
	void UpdateTimeRemaining();
	void EvaluateRemovalAfterDebuffRemoved();
	void RemoveStatusEffectWidget();
	void BindGameplayListeners();
	void UnbindGameplayListeners();
	void ClearUpdateTimeRemainingTimer();
	void OnDebuffTagChanged(FGameplayTag CallbackTag, int32 NewCount);
	void OnStatusEffectTagChanged(FGameplayTag CallbackTag, int32 NewCount);
	void OnReplicatedStatusEffectStackChanged(FGameplayTag DebuffTag, int32 StackCount);

	UAbilitySystemComponent* GetOwnerAbilitySystemComponent() const;
	int32 GetMaxStackCount() const;
	float GetStatusDuration() const;
	int32 GetActiveDebuffStackCount() const;

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> BoundAbilitySystemComponent;

	UPROPERTY(Transient)
	TObjectPtr<UStatusEffectReplicationComponent> BoundStatusEffectReplicationComponent;

	FGameplayTag BoundDebuffTag;
	FGameplayTag BoundStatusEffectTag;
	FDelegateHandle DebuffTagChangedHandle;
	FDelegateHandle StatusEffectTagChangedHandle;
	FDelegateHandle ReplicatedStackChangedHandle;
	FTimerHandle StackFillHoldTimer;
	FTimerHandle UpdateStackFillTimer;
	FTimerHandle UpdateTimeRemainingTimer;
	double StackFillDecreaseStartTime = 0.0;
	float StackFillDecreaseStartPercent = 0.0f;
	bool bIsConstructed = false;
	bool bIsStatusEffectApplied = false;
};
