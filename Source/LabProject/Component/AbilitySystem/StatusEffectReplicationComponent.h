#pragma once

#include "ActiveGameplayEffectHandle.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Iris/ReplicationState/IrisFastArraySerializer.h"
#include "TimerManager.h"

#include "StatusEffectReplicationComponent.generated.h"

class UAbilitySystemComponent;
class UStatusEffectReplicationComponent;
class UStatusEffectDefinition;
struct FActiveGameplayEffect;

DECLARE_MULTICAST_DELEGATE_TwoParams(
	FOnStatusEffectStackChanged,
	FGameplayTag,
	int32);

USTRUCT()
struct LABPROJECT_API FReplicatedStatusEffectStackEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

public:
	// Event Handlers --------------------------------------------------------------------------------------------------
	void PostReplicatedAdd(const struct FReplicatedStatusEffectStackList& InArraySerializer);
	void PostReplicatedChange(const struct FReplicatedStatusEffectStackList& InArraySerializer);
	void PreReplicatedRemove(const struct FReplicatedStatusEffectStackList& InArraySerializer);

public:
	UPROPERTY()
	FGameplayTag DebuffTag;

	UPROPERTY()
	int32 StackCount = 0;
};

USTRUCT()
struct LABPROJECT_API FReplicatedStatusEffectStackList : public FIrisFastArraySerializer
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<
			FReplicatedStatusEffectStackEntry,
			FReplicatedStatusEffectStackList>(Entries, DeltaParms, *this);
	}

	void MarkEntryDirty(FReplicatedStatusEffectStackEntry& Entry)
	{
		MarkItemDirty(Entry);
	}

public:
	UPROPERTY()
	TArray<FReplicatedStatusEffectStackEntry> Entries;

	UPROPERTY(NotReplicated)
	TObjectPtr<UStatusEffectReplicationComponent> Owner = nullptr;
};

template<>
struct TStructOpsTypeTraits<FReplicatedStatusEffectStackList>
	: public TStructOpsTypeTraitsBase2<FReplicatedStatusEffectStackList>
{
	enum { WithNetDeltaSerializer = true };
};

/**
 * Replicates only the compact status-stack data required by presentation.
 *
 * Enemy ability systems use Minimal gameplay-effect replication, so remote UI
 * cannot inspect their ActiveGameplayEffects FastArray. The server mirrors
 * relevant aggregate stack counts here while gameplay authority remains in GAS.
 */
UCLASS(ClassGroup=(AbilitySystem), meta=(BlueprintSpawnableComponent))
class LABPROJECT_API UStatusEffectReplicationComponent : public UActorComponent
{
	GENERATED_BODY()

private:
	friend struct FReplicatedStatusEffectStackEntry;

	struct FTrackedActiveEffectBinding
	{
		FGameplayTag DebuffTag;
		FDelegateHandle StackChangedHandle;
	};

	struct FStatusEffectDecayState
	{
		FActiveGameplayEffectHandle ActiveEffectHandle;
		FTimerHandle UpdateTimerHandle;
		double DecayStartTime = 0.0;
		int32 StartingStackCount = 0;
		int32 MaxStackCount = 0;
	};

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Public API ------------------------------------------------------------------------------------------------------
	UStatusEffectReplicationComponent(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	void SetStatusEffectStackCount(
		FGameplayTag DebuffTag,
		int32 StackCount,
		FActiveGameplayEffectHandle ActiveEffectHandle = FActiveGameplayEffectHandle());
	void TrackAppliedStatusEffect(
		const UStatusEffectDefinition* StatusEffectDefinition,
		FActiveGameplayEffectHandle ActiveEffectHandle);

	UFUNCTION(BlueprintPure, Category = "!AbilitySystem|StatusEffect")
	int32 GetStatusEffectStackCount(FGameplayTag DebuffTag) const;

	// Event Handlers --------------------------------------------------------------------------------------------------
	FOnStatusEffectStackChanged& OnStatusEffectStackChanged()
	{
		return StatusEffectStackChanged;
	}

private:
	void HandleReplicatedStackAddedOrChanged(
		const FReplicatedStatusEffectStackEntry& Entry);
	void HandleReplicatedStackRemoved(FGameplayTag DebuffTag);
	void RefreshReplicatedStackFromAbilitySystem(FGameplayTag DebuffTag);
	void UpdateStatusEffectDecay(FGameplayTag DebuffTag);

	void HandleDebuffTagChanged(FGameplayTag DebuffTag, int32 NewCount);
	void HandleActiveEffectStackChanged(
		FActiveGameplayEffectHandle ActiveEffectHandle,
		int32 NewStackCount,
		int32 PreviousStackCount);
	void HandleAnyActiveEffectRemoved(const FActiveGameplayEffect& RemovedEffect);

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void NotifyStatusEffectStackChanged(FGameplayTag DebuffTag, int32 StackCount);

	void EnsureAbilitySystemBinding();
	void UnbindAbilitySystem();
	void TrackDebuffTag(FGameplayTag DebuffTag);
	void TrackActiveEffect(
		FGameplayTag DebuffTag,
		FActiveGameplayEffectHandle ActiveEffectHandle);
	void RefreshTrackedActiveEffects(FGameplayTag DebuffTag);
	int32 CalculateStackCountFromAbilitySystem(FGameplayTag DebuffTag) const;
	void WriteReplicatedStack(FGameplayTag DebuffTag, int32 StackCount);
	void MarkReplicatedStacksDirty();
	int32 FindReplicatedStackIndex(FGameplayTag DebuffTag) const;
	void RestartStatusEffectDecay(
		const UStatusEffectDefinition* StatusEffectDefinition,
		FActiveGameplayEffectHandle ActiveEffectHandle,
		int32 StackCount);
	void ClearStatusEffectDecay(FGameplayTag DebuffTag);
	void ClearAllStatusEffectDecayTimers();

private:
	UPROPERTY(Replicated)
	FReplicatedStatusEffectStackList ReplicatedStacks;

	TWeakObjectPtr<UAbilitySystemComponent> BoundAbilitySystemComponent;
	TMap<FGameplayTag, FDelegateHandle> DebuffTagChangedHandles;
	TMap<FActiveGameplayEffectHandle, FTrackedActiveEffectBinding> TrackedActiveEffects;
	TMap<FGameplayTag, FStatusEffectDecayState> StatusEffectDecayStates;
	FDelegateHandle ActiveEffectRemovedHandle;
	FOnStatusEffectStackChanged StatusEffectStackChanged;
};
