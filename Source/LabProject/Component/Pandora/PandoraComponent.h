#pragma once

#include "CoreMinimal.h"
#include "Iris/ReplicationState/IrisFastArraySerializer.h"
#include "Logging/LogRateLimiter.h"
#include "UObject/PrimaryAssetId.h"
#include "Components/PlayerStateComponent.h"
#include "Pandora/PandoraLoadoutTypes.h"
#include "Pandora/PandoraSkillSourceOwner.h"
#include "GameplayAbilitySpecHandle.h"
#include "PandoraComponent.generated.h"

class UPandoraComponent;
class UPandoraDefinition;
class UPandoraSkillSource;
class UPdAbilitySystemComponent;
class UItemDefinition;
class UEquipmentComponent;
struct FStreamableHandle;
struct FGameplayAbilitySpec;

DECLARE_LOG_CATEGORY_EXTERN(PandoraComponentLog, Log, All);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPdPandoraSelectionChangedDelegate, const UPandoraDefinition*, PandoraDefinition);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPdPandoraLoadoutChangedDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPdPandoraInventoryChangedDelegate);

USTRUCT()
struct LABPROJECT_API FReplicatedPandoraEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

public:
	// Event Handlers --------------------------------------------------------------------------------------------------
	void PostReplicatedAdd(const struct FReplicatedPandoraList& InArraySerializer);
	void PostReplicatedChange(const struct FReplicatedPandoraList& InArraySerializer);
	void PreReplicatedRemove(const struct FReplicatedPandoraList& InArraySerializer);

public:
	UPROPERTY()
	TObjectPtr<const UPandoraDefinition> PandoraDefinition = nullptr;
};

USTRUCT()
struct LABPROJECT_API FReplicatedPandoraList : public FIrisFastArraySerializer
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FReplicatedPandoraEntry, FReplicatedPandoraList>(Entries, DeltaParms, *this);
	}

	void MarkEntryDirty(FReplicatedPandoraEntry& Entry)
	{
		MarkItemDirty(Entry);
	}

	// Event Handlers --------------------------------------------------------------------------------------------------
	void PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters);

public:
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
class LABPROJECT_API UPandoraComponent : public UPlayerStateComponent, public IPandoraSkillSourceOwner
{
	GENERATED_BODY()

private:
	friend struct FReplicatedPandoraEntry;
	friend struct FReplicatedPandoraList;

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void BeginPlay() override;
	virtual void ReadyForReplication() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Public API ------------------------------------------------------------------------------------------------------
	UPandoraComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	void GrantPandorasByPrimaryAssetIds(const TArray<FPrimaryAssetId>& PandoraDefinitions);

	void GrantPandorasWithLoadout(
		const TArray<FPrimaryAssetId>& PandoraDefinitions,
		const TMap<EEnum_Direction, FPrimaryAssetId>& PandoraLoadoutByDirection);

	void ClearAllPandoras();

	bool GrantPandoraDefinition(const UPandoraDefinition* PandoraDefinition);

	UFUNCTION(BlueprintPure, Category = "!Pandora")
	bool HasPandoraDefinition(const UPandoraDefinition* PandoraDefinition) const;

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

	UFUNCTION(BlueprintPure, Category = "!Pandora|Skill")
	const UPandoraDefinition* GetCurrentPandoraDefinition() const { return CurrentPandoraDefinition; }

	UFUNCTION(BlueprintPure, Category = "!Pandora|Skill")
	EEnum_Direction GetCurrentPandoraLoadoutDirection() const { return CurrentPandoraLoadoutDirection; }

	void RefreshCurrentPandoraSkills();

	UFUNCTION(BlueprintPure, Category = "!Pandora|Weapon")
	bool IsPandoraCompatibleWithCurrentWeapon(const UPandoraDefinition* PandoraDefinition) const;

protected:
	// Network RPCs ----------------------------------------------------------------------------------------------------
	UFUNCTION(Server, Reliable)
	void ServerRequestPandoraSelection(FPrimaryAssetId PandoraDefinitionId, EEnum_Direction RequestedDirection);

	UFUNCTION(Server, Reliable)
	void ServerSetPandoraLoadoutSlot(EEnum_Direction Direction, FPrimaryAssetId PandoraDefinitionId);

	UFUNCTION(Server, Reliable)
	void ServerAutoSetPandoraLoadoutSlot(FPrimaryAssetId PandoraDefinitionId);

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	UFUNCTION()
	void OnRep_CurrentPandoraDefinition();

	UFUNCTION()
	void OnRep_CurrentPandoraLoadoutDirection();

	UFUNCTION()
	void OnRep_PandoraLoadoutSlots();

	void HandleGrantedAbilityRemoved(const FGameplayAbilitySpec& Spec);
	virtual void HandleSkillSourceReplicated(UPandoraSkillSource* Source) override;
	virtual void HandleSkillSourceDestroyed(UPandoraSkillSource* Source) override;

	// Internal Helpers ------------------------------------------------------------------------------------------------
	bool AddOwnedPandoraEntry(const UPandoraDefinition* PandoraDefinition);
	void CancelPendingPandoraLoads();
	bool SelectPandoraByPrimaryAssetId(FPrimaryAssetId PandoraDefinitionId, EEnum_Direction RequestedDirection = EEnum_Direction::Center);
	void ClearGrantedPandoraContent();
	bool HasPandoraAuthority() const;
	int32 ResolveSelectedPandoraRuntimeLevel(const UPandoraDefinition* PandoraDefinition) const;
	EEnum_Direction ResolvePandoraSelectionDirection(const UPandoraDefinition* PandoraDefinition, EEnum_Direction RequestedDirection) const;
	UPdAbilitySystemComponent* GetOwnerAbilitySystemComponent() const;
	const UEquipmentComponent* GetCurrentEquipmentComponent() const;
	const UItemDefinition* GetCurrentWeaponDefinition() const;
	EEnum_Direction GetCurrentWeaponLoadoutDirection() const;
	const UPandoraDefinition* FindOwnedPandoraDefinitionByPrimaryAssetId(FPrimaryAssetId PandoraDefinitionId) const;
	int32 FindReplicatedEntryIndexByDefinition(const UPandoraDefinition* PandoraDefinition) const;
	FReplicatedPandoraEntry* FindReplicatedEntryByDefinition(const UPandoraDefinition* PandoraDefinition);
	const FReplicatedPandoraEntry* FindReplicatedEntryByDefinition(const UPandoraDefinition* PandoraDefinition) const;

	void NotifyPandoraSelectionChanged();
	void NotifyPandoraLoadoutChanged();
	bool ResolveAutoPandoraLoadoutDirection(const UPandoraDefinition* PandoraDefinition, EEnum_Direction& OutDirection) const;
	bool ResolvePreferredAutoPandoraLoadoutDirection(const UPandoraDefinition* PandoraDefinition, EEnum_Direction& OutDirection) const;
	bool SetPandoraLoadoutSlotInternal(EEnum_Direction Direction, const UPandoraDefinition* PandoraDefinition);
	void LogRejectedServerRequest(const TCHAR* RequestName, const FString& Reason);

	// 능력 부여 전에 출처를 초기화하고, 컴포넌트가 보관과 복제 수명을 맡는다.
	UPandoraSkillSource* CreateSkillSource(const UPandoraDefinition* Definition, int32 SkillIndex,
		EEnum_Direction LoadoutDirection);
	void ReleaseSkillSourceIfUnused(UPandoraSkillSource* Source,
		FGameplayAbilitySpecHandle RemovedHandle = FGameplayAbilitySpecHandle());

	void GrantPandoraSkills(UPdAbilitySystemComponent* ASC, const UPandoraDefinition* Definition, int32 PandoraLevel, EEnum_Direction LoadoutDirection);
	void RemoveGrantedPandoraSkills(UPdAbilitySystemComponent* ASC, const TArray<FGameplayAbilitySpecHandle>& AbilityHandles);
	void RefreshPandoraSkillInputBindings(UPdAbilitySystemComponent* ASC) const;
	void BindAbilityRemoval();
	void ReleaseSkillSourcesForEndPlay();

public:
	UPROPERTY(BlueprintAssignable, Category = "!Inventory")
	FPdPandoraInventoryChangedDelegate OnPandoraInventoryChanged;

	UPROPERTY(BlueprintAssignable, Category = "!Pandora|Skill")
	FPdPandoraSelectionChangedDelegate OnPandoraSelectionChanged;

	UPROPERTY(BlueprintAssignable, Category = "!Pandora|Loadout")
	FPdPandoraLoadoutChangedDelegate OnPandoraLoadoutChanged;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UPandoraSkillSource>> OwnedSkillSources;
	TWeakObjectPtr<UPdAbilitySystemComponent> SourceAbilitySystemComponent;
	FDelegateHandle AbilityRemovedHandle;

	uint64 PandoraLoadGeneration = 0;
	bool bReplicatedInventoryChanged = false;
	TArray<TSharedPtr<FStreamableHandle>> PendingPandoraLoadHandles;

protected:
	UPROPERTY(Replicated)
	FReplicatedPandoraList ReplicatedEntries;

	UPROPERTY(Transient)
	TArray<FGameplayAbilitySpecHandle> GrantedPandoraAbilityHandles;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentPandoraDefinition)
	TObjectPtr<const UPandoraDefinition> CurrentPandoraDefinition;

	// 빈 슬롯을 선택해도 방향은 유지한다. 판도라 선택 여부는 CurrentPandoraDefinition으로 판단한다.
	UPROPERTY(ReplicatedUsing = OnRep_CurrentPandoraLoadoutDirection)
	EEnum_Direction CurrentPandoraLoadoutDirection = EEnum_Direction::Center;

	UPROPERTY(ReplicatedUsing = OnRep_PandoraLoadoutSlots)
	TArray<FPandoraLoadoutSlot> PandoraLoadoutSlots;

	FLogRateLimiter ServerValidationLogLimiter;
};
