#pragma once

#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"

#include "StatusEffectsBarWidget.generated.h"

class AActor;
class UAbilitySystemComponent;
class UHorizontalBox;
class UPdStatusEffectDataAsset;
class UUserWidget;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UStatusEffectsBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UStatusEffectsBarWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "!UI|StatusEffect")
	void SetOwnerActor(AActor* InOwnerActor);

	UFUNCTION(BlueprintCallable, Category = "!UI|StatusEffect")
	void TryAddStatusEffectWidget(UPdStatusEffectDataAsset* DataAsset);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "!UI|StatusEffect")
	bool AlreadyDisplayingStatusEffect(FGameplayTag DebuffTag) const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|StatusEffect|Widgets")
	TObjectPtr<UHorizontalBox> HorizontalBox;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ExposeOnSpawn = "true"), Category = "!UI|StatusEffect")
	TObjectPtr<AActor> OwnerActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|StatusEffect")
	TSubclassOf<UUserWidget> StatusEffectWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|StatusEffect")
	TArray<TObjectPtr<UPdStatusEffectDataAsset>> StatusEffectDataAssets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|StatusEffect")
	FMargin StatusEffectWidgetPadding = FMargin(5.0f, 0.0f);

private:
	void ScheduleStatusEffectTagBinding();
	void BindStatusEffectTagDelegates();
	void UnbindStatusEffectTagDelegates();
	void HandleObservedTagChanged(FGameplayTag CallbackTag, int32 NewCount);
	void AddWidgetsForExistingTags();
	UUserWidget* CreateStatusEffectWidget() const;
	UAbilitySystemComponent* GetOwnerAbilitySystemComponent() const;
	UPdStatusEffectDataAsset* FindDataAssetForObservedTag(FGameplayTag Tag) const;

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> BoundAbilitySystemComponent;

	TMap<FGameplayTag, FDelegateHandle> ObservedTagChangedHandles;
	FTimerHandle BindStatusEffectTagsTimerHandle;
	int32 BindRetryCount = 0;
	bool bBindStatusEffectTagsScheduled = false;
	bool bIsConstructed = false;

	static constexpr int32 MaxBindRetryCount = 20;
};
