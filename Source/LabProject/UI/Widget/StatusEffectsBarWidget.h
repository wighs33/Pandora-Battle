#pragma once

#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"

#include "StatusEffectsBarWidget.generated.h"

class AActor;
class UAbilitySystemComponent;
class UHorizontalBox;
class UStatusEffectDefinition;
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
	void TryAddStatusEffectWidget(UStatusEffectDefinition* DataAsset);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "!UI|StatusEffect")
	bool AlreadyDisplayingStatusEffect(FGameplayTag DebuffTag) const;

	UFUNCTION(BlueprintCallable, Category = "!UI|StatusEffect")
	void RefreshStatusEffectWidgets();

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
	TArray<TObjectPtr<UStatusEffectDefinition>> StatusEffectDataAssets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|StatusEffect")
	FMargin StatusEffectWidgetPadding = FMargin(5.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|StatusEffect|Layout")
	float StatusEffectWidgetVerticalOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|StatusEffect|Layout", meta = (ClampMin = "0.01"))
	float StatusEffectWidgetScale = 1.0f;

private:
	void ApplyWidgetDefinitionSettings();
	void BeginStatusEffectContentPreload();
	void ReleaseStatusEffectContentPreload();
	void ScheduleStatusEffectTagBinding();
	void ScheduleStatusEffectWidgetRefresh();
	void BindStatusEffectTagDelegates();
	void UnbindStatusEffectTagDelegates();
	void HandleObservedTagChanged(FGameplayTag CallbackTag, int32 NewCount);
	void AddWidgetsForExistingTags();
	bool IsStatusEffectActive(const UStatusEffectDefinition* DataAsset) const;
	UUserWidget* CreateStatusEffectWidget();
	void ResolveStatusEffectWidgetClass();
	UAbilitySystemComponent* GetOwnerAbilitySystemComponent() const;
	void GatherObservedStatusEffectDataAssets(TArray<UStatusEffectDefinition*>& OutDataAssets);
	void RebuildObservedStatusEffectDataAssetCache();
	void AddObservedStatusEffectDataAsset(UStatusEffectDefinition* DataAsset);
	void InvalidateObservedStatusEffectDataAssetCache();

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> BoundAbilitySystemComponent;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStatusEffectDefinition>> CachedObservedStatusEffectDataAssets;

	TMap<FGameplayTag, FDelegateHandle> ObservedTagChangedHandles;
	FTimerHandle BindStatusEffectTagsTimerHandle;
	FTimerHandle RefreshStatusEffectWidgetsTimerHandle;
	int32 BindRetryCount = 0;
	bool bBindStatusEffectTagsScheduled = false;
	bool bStatusEffectWidgetRefreshScheduled = false;
	bool bIsConstructed = false;
	bool bObservedStatusEffectDataAssetCacheValid = false;
	int32 ContentPreloadGeneration = 0;

	static constexpr int32 MaxBindRetryCount = 20;
};
