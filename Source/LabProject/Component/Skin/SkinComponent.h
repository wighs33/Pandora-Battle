#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Iris/ReplicationState/IrisFastArraySerializer.h"
#include "UObject/PrimaryAssetId.h"
#include "Components/PlayerStateComponent.h"
#include "SkinComponent.generated.h"

class USkinComponent;
class USkinDefinition;
class USkinInstance;
class UProjectTagConfig;
struct FStreamableHandle;

DECLARE_LOG_CATEGORY_EXTERN(SkinComponentLog, Log, All);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPdSkinsChangedDelegate);

USTRUCT(BlueprintType, Blueprintable)
struct FSkinList
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "!Inventory")
	TArray<TObjectPtr<USkinInstance>> Skins;
};

USTRUCT()
struct LABPROJECT_API FReplicatedSkinEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

public:
	void PostReplicatedAdd(const struct FReplicatedSkinList& InArraySerializer);
	void PostReplicatedChange(const struct FReplicatedSkinList& InArraySerializer);
	void PreReplicatedRemove(const struct FReplicatedSkinList& InArraySerializer);

	UPROPERTY()
	TObjectPtr<const USkinDefinition> SkinDefinition = nullptr;
};

USTRUCT()
struct LABPROJECT_API FReplicatedSkinList : public FIrisFastArraySerializer
{
	GENERATED_BODY()

public:
	void PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters);

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FReplicatedSkinEntry, FReplicatedSkinList>(Entries, DeltaParms, *this);
	}

	void MarkEntryDirty(FReplicatedSkinEntry& Entry)
	{
		MarkItemDirty(Entry);
	}

	UPROPERTY()
	TArray<FReplicatedSkinEntry> Entries;

	UPROPERTY(NotReplicated)
	TObjectPtr<USkinComponent> Owner = nullptr;
};

template<>
struct TStructOpsTypeTraits<FReplicatedSkinList> : public TStructOpsTypeTraitsBase2<FReplicatedSkinList>
{
	enum { WithNetDeltaSerializer = true };
};

/**
 * 플레이어가 보유한 스킨 목록과 복제를 관리한다.
 *
 * 실제 장착과 외형 적용은 캐릭터의 스킨 장착 컴포넌트가 담당한다.
 */
UCLASS(BlueprintType, Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LABPROJECT_API USkinComponent : public UPlayerStateComponent
{
	GENERATED_BODY()

	friend struct FReplicatedSkinEntry;
	friend struct FReplicatedSkinList;

public:
	USkinComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// Engine Callbacks
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "!Inventory")
	void AddSkinsByPrimaryAssetIds(const TArray<FPrimaryAssetId>& SkinDefinitions);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "!Inventory")
	void AddSkinDefinitions(const TArray<USkinDefinition*>& SkinDefinitions);

	void ApplyProjectTagConfig(const UProjectTagConfig* ProjectTagConfig);

	UFUNCTION(BlueprintPure, Category = "!Inventory")
	bool HasSkinDefinition(const USkinDefinition* SkinDefinition) const;

	const FSkinList& GetAllSkins() const { return AllSkinList; }
	const TMap<FGameplayTag, FSkinList>& GetFilteredSkinMap() const { return Map_Type_SkinList; }

	UPROPERTY(BlueprintAssignable, Category = "!Inventory")
	FPdSkinsChangedDelegate OnSkinsChanged;

protected:
	void HandleReplicatedEntryAddedOrChanged(const FReplicatedSkinEntry& Entry);
	void HandleReplicatedEntryRemoved(const USkinDefinition* SkinDefinition);

	void RebuildRuntimeSkinsFromReplicatedEntries();
	void RebuildFilteredSkinMap();
	void FilterSkin(USkinInstance* SkinInstance);
	bool AddSkinDefinition(const USkinDefinition* SkinDefinition);
	bool HasSkinAuthority() const;
	void NotifySkinsChanged();
	USkinInstance* FindSkinInstanceByDefinition(const USkinDefinition* SkinDefinition) const;
	int32 FindReplicatedEntryIndexByDefinition(const USkinDefinition* SkinDefinition) const;
	FReplicatedSkinEntry* FindReplicatedEntryByDefinition(const USkinDefinition* SkinDefinition);
	const FReplicatedSkinEntry* FindReplicatedEntryByDefinition(const USkinDefinition* SkinDefinition) const;

private:
	UPROPERTY(Transient, BlueprintReadOnly, Category = "!Inventory|Filter", meta = (AllowPrivateAccess = "true"))
	TArray<FGameplayTag> FilterTypeTags;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!Inventory", meta = (AllowPrivateAccess = "true"))
	FSkinList AllSkinList;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!Inventory", meta = (AllowPrivateAccess = "true"))
	TMap<FGameplayTag, FSkinList> Map_Type_SkinList;

	uint64 SkinLoadGeneration = 0;
	bool bReplicatedInventoryChanged = false;
	TArray<TSharedPtr<FStreamableHandle>> PendingSkinLoadHandles;

protected:
	UPROPERTY(Replicated)
	FReplicatedSkinList ReplicatedEntries;
};
