#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Iris/ReplicationState/IrisFastArraySerializer.h"
#include "Logging/PdLogRateLimiter.h"
#include "UObject/PrimaryAssetId.h"
#include "Components/PlayerStateComponent.h"
#include "Pandora/PandoraLoadoutTypes.h"
#include "Pandora/PandoraSelectedContent.h"
#include "PandoraComponent.generated.h"

class UPandoraComponent;
class UPandoraDefinition;
class UPandoraInstance;
class UProjectTagConfig;
class UItemDefinition;
class ACharacterBase;
struct FStreamableHandle;

DECLARE_LOG_CATEGORY_EXTERN(PandoraComponentLog, Log, All);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPdPandoraSelectionChangedDelegate, UPandoraDefinition*, PandoraDefinition);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPdPandoraLoadoutChangedDelegate);

USTRUCT(BlueprintType, Blueprintable)
struct FPandoraList
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "!Inventory")
	TArray<TObjectPtr<UPandoraInstance>> Pandoras;
};

USTRUCT()
struct LABPROJECT_API FReplicatedPandoraEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

public:
	void PostReplicatedAdd(const struct FReplicatedPandoraList& InArraySerializer);
	void PostReplicatedChange(const struct FReplicatedPandoraList& InArraySerializer);
	void PreReplicatedRemove(const struct FReplicatedPandoraList& InArraySerializer);

	UPROPERTY()
	TObjectPtr<const UPandoraDefinition> PandoraDefinition = nullptr;

	UPROPERTY()
	bool IsOwned = false;
};

USTRUCT()
struct LABPROJECT_API FReplicatedPandoraList : public FIrisFastArraySerializer
{
	GENERATED_BODY()

public:
	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FReplicatedPandoraEntry, FReplicatedPandoraList>(Entries, DeltaParms, *this);
	}

	void MarkEntryDirty(FReplicatedPandoraEntry& Entry)
	{
		MarkItemDirty(Entry);
	}

	UPROPERTY()
	TArray<FReplicatedPandoraEntry> Entries;

	UPROPERTY(NotReplicated)
	TObjectPtr<UPandoraComponent> Owner = nullptr;
};

template<>
struct TStructOpsTypeTraits<FReplicatedPandoraList> : public TStructOpsTypeTraitsBase2<FReplicatedPandoraList>
{
	enum { WithNetDeltaSerializer = true };
};

UCLASS(BlueprintType, Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LABPROJECT_API UPandoraComponent : public UPlayerStateComponent
{
	GENERATED_BODY()

	friend struct FReplicatedPandoraEntry;

public:
	UPandoraComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// Timing hooks
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "!Inventory")
	void AddPandorasByPrimaryAssetIds(const TArray<FPrimaryAssetId>& PandoraDefinitions);

	UFUNCTION(BlueprintCallable, Category = "!Inventory")
	void ActivatePandoras(const TArray<FPrimaryAssetId>& PandoraDefinitions);

	void ActivatePandorasWithLoadout(
		const TArray<FPrimaryAssetId>& PandoraDefinitions,
		const TMap<EEnum_Direction, FPrimaryAssetId>& PandoraLoadoutByDirection);

	UFUNCTION()
	void ClearAllPandoras();

	UFUNCTION(BlueprintCallable, Category = "!Inventory")
	void FilterPandoras(UPandoraInstance* PandoraInstance);

	UFUNCTION(BlueprintCallable, Category = "!Inventory")
	void AddValueToMap(FGameplayTag TypeTag, UPandoraInstance* PandoraInstance);

	UFUNCTION(BlueprintCallable, Category = "!Pandora|Skill")
	bool RequestPandoraSelection(const UPandoraDefinition* PandoraDefinition);

	UFUNCTION(BlueprintCallable, Category = "!Pandora|Skill")
	bool RequestPandoraSelectionForDirection(EEnum_Direction Direction, const UPandoraDefinition* PandoraDefinition);

	UFUNCTION(BlueprintCallable, Category = "!Pandora|Loadout")
	bool RequestSetPandoraLoadoutSlot(EEnum_Direction Direction, const UPandoraDefinition* PandoraDefinition);

	UFUNCTION(BlueprintCallable, Category = "!Pandora|Loadout")
	bool RequestAutoSetPandoraLoadoutSlot(const UPandoraDefinition* PandoraDefinition);
	UFUNCTION(BlueprintPure, Category = "!Pandora|Loadout")
	const UPandoraDefinition* GetPandoraLoadoutDefinition(EEnum_Direction Direction) const;

	UFUNCTION(BlueprintPure, Category = "!Pandora|Loadout")
	UPandoraInstance* GetPandoraLoadoutInstance(EEnum_Direction Direction) const;
	UFUNCTION(BlueprintPure, Category = "!Pandora|Skill")
	const UPandoraDefinition* GetCurrentPandoraDefinition() const { return CurrentPandoraDefinition; }

	UFUNCTION(BlueprintPure, Category = "!Pandora|Skill")
	EEnum_Direction GetCurrentPandoraLoadoutDirection() const { return CurrentPandoraLoadoutDirection; }

	UPROPERTY(BlueprintAssignable, Category = "!Pandora|Skill")
	FPdPandoraSelectionChangedDelegate OnPandoraSelectionChanged;

	UPROPERTY(BlueprintAssignable, Category = "!Pandora|Loadout")
	FPdPandoraLoadoutChangedDelegate OnPandoraLoadoutChanged;

	UFUNCTION(BlueprintCallable, Category = "!Pandora")
	void RefreshSelectedPandoraAbilityBindings(class UPdAbilitySystemComponent* AbilitySystemComponent = nullptr) const;

	UFUNCTION(BlueprintCallable, Category = "!Pandora|Skill")
	void RefreshCurrentPandoraForWeaponChange();

	UFUNCTION(BlueprintPure, Category = "!Pandora|Weapon")
	bool IsPandoraCompatibleWithCurrentWeapon(const UPandoraDefinition* PandoraDefinition) const;

	void ApplyProjectTagConfig(const UProjectTagConfig* ProjectTagConfig);

protected:
	// Replication timing callbacks
	void HandleReplicatedEntryAddedOrChanged(const FReplicatedPandoraEntry& Entry);
	void HandleReplicatedEntryRemoved(const UPandoraDefinition* PandoraDefinition);

	// State rebuild helpers
	void InitializeReplicatedEntriesFromRuntimePandoras();
	void RebuildRuntimePandorasFromReplicatedEntries();
	void RebuildFilteredPandoraMap();
	void AddReplicatedPandora(UPandoraInstance* PandoraInstance);
	bool SelectPandoraByPrimaryAssetId(FPrimaryAssetId PandoraDefinitionId, EEnum_Direction RequestedDirection = EEnum_Direction::Center);
	void ClearSelectedPandoraContent();
	bool HasPandoraAuthority() const;
	int32 ResolveSelectedPandoraRuntimeLevel(const UPandoraDefinition* PandoraDefinition) const;
	EEnum_Direction ResolvePandoraSelectionDirection(const UPandoraDefinition* PandoraDefinition, EEnum_Direction RequestedDirection) const;
	const ACharacterBase* ResolveCurrentCharacterOwner() const;
	const class UEquipmentComponent* GetCurrentEquipmentComponent() const;
	const UItemDefinition* GetCurrentWeaponDefinition() const;
	EEnum_Direction GetCurrentWeaponLoadoutDirection() const;
	UPandoraInstance* FindPandoraInstanceByDefinition(const UPandoraDefinition* PandoraDefinition) const;
	UPandoraInstance* FindPandoraInstanceByPrimaryAssetId(FPrimaryAssetId PandoraDefinitionId) const;
	UPandoraInstance* FindOwnedPandoraInstanceByPrimaryAssetId(FPrimaryAssetId PandoraDefinitionId) const;
	int32 FindReplicatedEntryIndexByDefinition(const UPandoraDefinition* PandoraDefinition) const;
	FReplicatedPandoraEntry* FindReplicatedEntryByDefinition(const UPandoraDefinition* PandoraDefinition);
	const FReplicatedPandoraEntry* FindReplicatedEntryByDefinition(const UPandoraDefinition* PandoraDefinition) const;

	UFUNCTION(Server, Reliable)
	void ServerRequestPandoraSelection(FPrimaryAssetId PandoraDefinitionId, EEnum_Direction RequestedDirection);

	UFUNCTION(Server, Reliable)
	void ServerSetPandoraLoadoutSlot(EEnum_Direction Direction, FPrimaryAssetId PandoraDefinitionId);

	UFUNCTION(Server, Reliable)
	void ServerAutoSetPandoraLoadoutSlot(FPrimaryAssetId PandoraDefinitionId);

	UFUNCTION()
	void OnRep_CurrentPandoraDefinition();

	UFUNCTION()
	void OnRep_CurrentPandoraLoadoutDirection();

	UFUNCTION()
	void OnRep_PandoraLoadoutSlots();

	void NotifyPandoraSelectionChanged();
	void NotifyPandoraLoadoutChanged();
	void RefreshPlacedPandoraSkillPreloads();
	void ReleasePlacedPandoraSkillPreloads();
	void TryAutoSelectPandoraForLoadoutSlot(EEnum_Direction Direction, const UPandoraDefinition* PandoraDefinition);
	bool ResolveAutoPandoraLoadoutDirection(const UPandoraDefinition* PandoraDefinition, EEnum_Direction& OutDirection) const;
	bool ResolvePreferredAutoPandoraLoadoutDirection(const UPandoraDefinition* PandoraDefinition, EEnum_Direction& OutDirection) const;
	bool SetPandoraLoadoutSlotInternal(EEnum_Direction Direction, const UPandoraDefinition* PandoraDefinition, bool bRequireOwnedPandora);
	FPandoraLoadoutSlot* FindPandoraLoadoutSlot(EEnum_Direction Direction);
	const FPandoraLoadoutSlot* FindPandoraLoadoutSlot(EEnum_Direction Direction) const;
	void LogRejectedServerRequest(const TCHAR* RequestName, const FString& Reason);

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Default", meta = (DisplayName = "All Pandroa Definition"))
	TArray<FPrimaryAssetId> AllPandroaDefinition;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!Inventory|Filter")
	TArray<FGameplayTag> FilterTypeTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "!Inventory")
	FPandoraList AllPandoraList;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "!Inventory")
	TMap<FGameplayTag, FPandoraList> Map_Type_PandoraList;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "!Pandora|Selection")
	bool bApplyPandoraContentOnSelection = true;

	UPROPERTY(Replicated)
	FReplicatedPandoraList ReplicatedEntries;

	UPROPERTY(Transient)
	FPandoraSelectedContent SelectedPandoraContent;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentPandoraDefinition)
	TObjectPtr<const UPandoraDefinition> CurrentPandoraDefinition;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentPandoraLoadoutDirection)
	EEnum_Direction CurrentPandoraLoadoutDirection = EEnum_Direction::Center;

	UPROPERTY(ReplicatedUsing = OnRep_PandoraLoadoutSlots)
	TArray<FPandoraLoadoutSlot> PandoraLoadoutSlots;

	mutable TWeakObjectPtr<ACharacterBase> CachedCharacterOwner;
	TMap<FPrimaryAssetId, TSharedPtr<FStreamableHandle>> PlacedPandoraSkillPreloadHandles;
	FPdLogRateLimiter ServerValidationLogLimiter;
};
