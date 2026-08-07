#pragma once

#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"

#include "StatusEffectsBarWidget.generated.h"

class AActor;
class UAbilitySystemComponent;
class UHorizontalBox;
class UStatusEffectDefinition;
class UStatusEffectReplicationComponent;
class UUserWidget;
struct FStreamableHandle;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UStatusEffectsBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UStatusEffectsBarWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "!UI|StatusEffect")
	void SetOwnerActor(AActor* InOwnerActor);

	void CenterHorizontalBox();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|StatusEffect|Widgets")
	TObjectPtr<UHorizontalBox> HorizontalBox;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ExposeOnSpawn = "true"), Category = "!UI|StatusEffect")
	TObjectPtr<AActor> OwnerActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|StatusEffect")
	TSubclassOf<UUserWidget> StatusEffectWidgetClass;

private:
	void TryAddStatusEffectWidget(UStatusEffectDefinition* DataAsset);
	void RefreshStatusEffectWidgets();
	void ApplyWidgetDefinitionSettings();
	void BeginStatusEffectContentPreload();
	void ReleaseStatusEffectContentPreload();
	void ScheduleStatusEffectTagBinding();
	void ScheduleStatusEffectWidgetRefresh();
	void BindStatusEffectTagDelegates();
	void UnbindStatusEffectTagDelegates();
	void HandleObservedTagChanged(FGameplayTag CallbackTag, int32 NewCount);
	void HandleReplicatedStatusEffectStackChanged(FGameplayTag DebuffTag, int32 StackCount);
	int32 GetStatusEffectDisplayCount(const UStatusEffectDefinition* DataAsset) const;
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
	TObjectPtr<UStatusEffectReplicationComponent> BoundStatusEffectReplicationComponent;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStatusEffectDefinition>> CachedObservedStatusEffectDataAssets;
	TSharedPtr<FStreamableHandle> StatusEffectContentPreloadHandle;

	TMap<FGameplayTag, FDelegateHandle> ObservedTagChangedHandles;
	FDelegateHandle ReplicatedStackChangedHandle;
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
