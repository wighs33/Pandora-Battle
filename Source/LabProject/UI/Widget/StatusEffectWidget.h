#pragma once

#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"

#include "StatusEffectWidget.generated.h"

class AActor;
class UAbilitySystemComponent;
class UImage;
class UStatusEffectDefinition;
class UProgressBar;
struct FGameplayEventData;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|StatusEffect")
	FVector2D IconImageSize = FVector2D(32.0f, 32.0f);

private:
	void ApplyWidgetDefinitionSettings();
	void InitializeStatusEffect();
	void ApplyDesignerDefaults();
	void SetInitialValues();
	void SetIconStyle();
	void RefreshFromActiveEffects();
	void UpdateFillMeter();
	void StartDecreaseFillMeterTimer();
	void DecreaseStackFill();
	void HandleStatusEffectApplied();
	void UpdateTimeRemaining();
	void EvaluateRemovalAfterDebuffRemoved();
	void RemoveStatusEffectWidget();
	void BindGameplayListeners();
	void UnbindGameplayListeners();
	void ClearDecreaseStackFillTimer();
	void ClearUpdateTimeRemainingTimer();
	void OnDebuffTagChanged(FGameplayTag CallbackTag, int32 NewCount);
	void OnStatusEffectTagChanged(FGameplayTag CallbackTag, int32 NewCount);
	void OnStackCountChangedEvent(const FGameplayEventData* Payload);

	UAbilitySystemComponent* GetOwnerAbilitySystemComponent() const;
	int32 GetMaxStackCount() const;
	float GetDebuffStackDuration() const;
	float GetStatusDuration() const;
	int32 GetActiveDebuffStackCount() const;

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> BoundAbilitySystemComponent;

	FGameplayTag BoundDebuffTag;
	FGameplayTag BoundStatusEffectTag;
	FDelegateHandle DebuffTagChangedHandle;
	FDelegateHandle StatusEffectTagChangedHandle;
	FDelegateHandle StackCountChangedEventHandle;
	FTimerHandle DecreaseStackFillTimer;
	FTimerHandle UpdateTimeRemainingTimer;
	bool bIsConstructed = false;
};
