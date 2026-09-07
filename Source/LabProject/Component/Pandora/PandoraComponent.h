#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Iris/ReplicationState/IrisFastArraySerializer.h"
#include "Logging/LogRateLimiter.h"
#include "UObject/PrimaryAssetId.h"
#include "Components/PlayerStateComponent.h"
#include "Pandora/PandoraLoadoutTypes.h"
#include "GameplayAbilitySpecHandle.h"
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
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPdPandoraInventoryChangedDelegate);

USTRUCT(BlueprintType, Blueprintable)
struct FPandoraList
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "!Inventory")
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
	void PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters);

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

/**
 * 플레이어의 판도라 보유 목록과 로드아웃을 관리한다.
 *
 * 성장과 포인트는 트리 컴포넌트에 맡기고, 선택 변경에서는 기존 능력을 유지한다.
 */
UCLASS(BlueprintType, Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LABPROJECT_API UPandoraComponent : public UPlayerStateComponent
{
	GENERATED_BODY()

	friend struct FReplicatedPandoraEntry;
	friend struct FReplicatedPandoraList;

public:
	UPandoraComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// Engine Callbacks
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

	bool GrantPandoraDefinition(const UPandoraDefinition* PandoraDefinition);

	UFUNCTION(BlueprintPure, Category = "!Pandora")
	bool HasPandoraDefinition(const UPandoraDefinition* PandoraDefinition) const;

	const FPandoraList& GetAllPandoras() const { return AllPandoraList; }
	const TMap<FGameplayTag, FPandoraList>& GetFilteredPandoraMap() const { return Map_Type_PandoraList; }

	UPROPERTY(BlueprintAssignable, Category = "!Inventory")
	FPdPandoraInventoryChangedDelegate OnPandoraInventoryChanged;

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
	void HandleReplicatedEntryAddedOrChanged(const FReplicatedPandoraEntry& Entry);
	void HandleReplicatedEntryRemoved(const UPandoraDefinition* PandoraDefinition);

	void RebuildRuntimePandorasFromReplicatedEntries();
	void RebuildFilteredPandoraMap();
	void FilterPandoras(UPandoraInstance* PandoraInstance);
	bool AddPandoraDefinition(const UPandoraDefinition* PandoraDefinition, bool bOwned);
	void LoadPandoraDefinitions(const TArray<FPrimaryAssetId>& PandoraDefinitions,
		const TMap<EEnum_Direction, FPrimaryAssetId>& PandoraLoadoutByDirection, bool bOwned);
	void CancelPendingPandoraLoads();
	bool SelectPandoraByPrimaryAssetId(FPrimaryAssetId PandoraDefinitionId, EEnum_Direction RequestedDirection = EEnum_Direction::Center);
	void ClearGrantedPandoraContent();
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
	bool ResolveAutoPandoraLoadoutDirection(const UPandoraDefinition* PandoraDefinition, EEnum_Direction& OutDirection) const;
	bool ResolvePreferredAutoPandoraLoadoutDirection(const UPandoraDefinition* PandoraDefinition, EEnum_Direction& OutDirection) const;
	bool SetPandoraLoadoutSlotInternal(EEnum_Direction Direction, const UPandoraDefinition* PandoraDefinition, bool bRequireOwnedPandora);
	FPandoraLoadoutSlot* FindPandoraLoadoutSlot(EEnum_Direction Direction);
	const FPandoraLoadoutSlot* FindPandoraLoadoutSlot(EEnum_Direction Direction) const;
	void LogRejectedServerRequest(const TCHAR* RequestName, const FString& Reason);

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Default", meta = (DisplayName = "All Pandroa Definition"))
	TArray<FPrimaryAssetId> AllPandroaDefinition;

private:
	UPROPERTY(Transient, BlueprintReadOnly, Category = "!Inventory|Filter", meta = (AllowPrivateAccess = "true"))
	TArray<FGameplayTag> FilterTypeTags;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!Inventory", meta = (AllowPrivateAccess = "true"))
	FPandoraList AllPandoraList;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!Inventory", meta = (AllowPrivateAccess = "true"))
	TMap<FGameplayTag, FPandoraList> Map_Type_PandoraList;

	uint64 PandoraLoadGeneration = 0;
	bool bReplicatedInventoryChanged = false;
	TArray<TSharedPtr<FStreamableHandle>> PendingPandoraLoadHandles;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "!Pandora|Selection")
	bool bApplyPandoraContentOnSelection = true;

	UPROPERTY(Replicated)
	FReplicatedPandoraList ReplicatedEntries;

	UPROPERTY(Transient)
	TArray<FGameplayAbilitySpecHandle> GrantedPandoraAbilityHandles;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentPandoraDefinition)
	TObjectPtr<const UPandoraDefinition> CurrentPandoraDefinition;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentPandoraLoadoutDirection)
	EEnum_Direction CurrentPandoraLoadoutDirection = EEnum_Direction::Center;

	UPROPERTY(ReplicatedUsing = OnRep_PandoraLoadoutSlots)
	TArray<FPandoraLoadoutSlot> PandoraLoadoutSlots;

	mutable TWeakObjectPtr<ACharacterBase> CachedCharacterOwner;
	TMap<FPrimaryAssetId, TSharedPtr<FStreamableHandle>> PlacedPandoraSkillPreloadHandles;
	FLogRateLimiter ServerValidationLogLimiter;
};
