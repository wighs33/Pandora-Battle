#pragma once

#include "Blueprint/UserWidget.h"
#include "Component/Character/AbilitySystemReadySubscription.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"

#include "StatusEffectsBarWidget.generated.h"

class AActor;
class ACharacterBase;
class UAbilitySystemComponent;
class UHorizontalBox;
class UStatusEffectDefinition;
class UStatusEffectReplicationComponent;
class UStatusEffectWidget;
struct FStreamableHandle;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UStatusEffectsBarWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

public:
	// Public API ------------------------------------------------------------------------------------------------------
	UStatusEffectsBarWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "!UI|StatusEffect")
	void SetOwnerActor(AActor* InOwnerActor);

	void CenterHorizontalBox();

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	void RefreshStatusEffectWidgets();
	void BindStatusEffectTagDelegates();
	void HandleObservedTagChanged(FGameplayTag CallbackTag, int32 NewCount);
	void HandleReplicatedStatusEffectStackChanged(FGameplayTag DebuffTag, int32 StackCount);
	void HandleOwnerAbilitySystemReady(ACharacterBase* Character, UPdAbilitySystemComponent* AbilitySystemComponent);

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void ObserveOwnerAbilitySystem();
	void TryAddStatusEffectWidget(UStatusEffectDefinition* DataAsset);
	void ApplyWidgetDefinitionSettings();
	void BeginStatusEffectContentPreload();
	void ReleaseStatusEffectContentPreload();
	void ScheduleStatusEffectWidgetRefresh();
	void UnbindStatusEffectTagDelegates();
	int32 GetStatusEffectDisplayCount(const UStatusEffectDefinition* DataAsset) const;
	UStatusEffectWidget* CreateStatusEffectWidget();
	void ResolveStatusEffectWidgetClass();
	UAbilitySystemComponent* GetOwnerAbilitySystemComponent() const;
	void GatherObservedStatusEffectDataAssets(TArray<UStatusEffectDefinition*>& OutDataAssets);
	void RebuildObservedStatusEffectDataAssetCache();
	void AddObservedStatusEffectDataAsset(UStatusEffectDefinition* DataAsset);
	void InvalidateObservedStatusEffectDataAssetCache();

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|StatusEffect|Widgets")
	TObjectPtr<UHorizontalBox> HorizontalBox;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ExposeOnSpawn = "true"), Category = "!UI|StatusEffect")
	TObjectPtr<AActor> OwnerActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|StatusEffect")
	TSubclassOf<UStatusEffectWidget> StatusEffectWidgetClass;

private:
	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> BoundAbilitySystemComponent;

	UPROPERTY(Transient)
	TObjectPtr<UStatusEffectReplicationComponent> BoundStatusEffectReplicationComponent;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStatusEffectDefinition>> CachedObservedStatusEffectDataAssets;
	TSharedPtr<FStreamableHandle> StatusEffectContentPreloadHandle;

	TMap<FGameplayTag, FDelegateHandle> ObservedTagChangedHandles;
	FDelegateHandle ReplicatedStackChangedHandle;
	FTimerHandle RefreshStatusEffectWidgetsTimerHandle;
	FAbilitySystemReadySubscription OwnerReadySubscription;
	bool bStatusEffectWidgetRefreshScheduled = false;
	bool bIsConstructed = false;
	bool bObservedStatusEffectDataAssetCacheValid = false;
	int32 ContentPreloadGeneration = 0;
};
