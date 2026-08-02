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

DECLARE_LOG_CATEGORY_EXTERN(SkinComponentLog, Log, All);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPdSkinsChangedDelegate);

USTRUCT(BlueprintType, Blueprintable)
struct FSkinList
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "!Inventory")
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

UCLASS(BlueprintType, Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LABPROJECT_API USkinComponent : public UPlayerStateComponent
{
	GENERATED_BODY()

	friend struct FReplicatedSkinEntry;

public:
	USkinComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// Timing hooks
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "!Inventory", meta = (DisplayName = "Make&AddSkins"))
	void MakeAndAddSkins(const TArray<FPrimaryAssetId>& SkinDefinitions);

	UFUNCTION(BlueprintCallable, Category = "!Inventory")
	void AddSkinsByPrimaryAssetIds(const TArray<FPrimaryAssetId>& SkinDefinitions);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "!Inventory")
	void AddSkinDefinitions(const TArray<USkinDefinition*>& SkinDefinitions);
	UFUNCTION(BlueprintCallable, Category = "!Inventory")
	void FilterSkin(USkinInstance* SkinInstance);

	UFUNCTION(BlueprintCallable, Category = "!Inventory")
	void AddValueToMap(FGameplayTag TypeTag, USkinInstance* SkinInstance);

	void ApplyProjectTagConfig(const UProjectTagConfig* ProjectTagConfig);

	UFUNCTION(BlueprintPure, Category = "!Inventory")
	bool HasSkinDefinition(const USkinDefinition* SkinDefinition) const;

	UPROPERTY()
	FPdSkinsChangedDelegate OnSkinsChanged;

protected:
	// Replication timing callbacks
	void HandleReplicatedEntryAddedOrChanged(const FReplicatedSkinEntry& Entry);
	void HandleReplicatedEntryRemoved(const USkinDefinition* SkinDefinition);

	// State rebuild helpers
	void InitializeReplicatedEntriesFromRuntimeSkins();
	void RebuildRuntimeSkinsFromReplicatedEntries();
	void RebuildFilteredSkinMap();
	void AddReplicatedSkin(USkinInstance* SkinInstance);
	bool HasSkinAuthority() const;
	bool CanReferenceSkinDefinition(const USkinDefinition* SkinDefinition) const;
	void NotifySkinsChanged();
	USkinInstance* FindSkinInstanceByDefinition(const USkinDefinition* SkinDefinition) const;
	int32 FindReplicatedEntryIndexByDefinition(const USkinDefinition* SkinDefinition) const;
	FReplicatedSkinEntry* FindReplicatedEntryByDefinition(const USkinDefinition* SkinDefinition);
	const FReplicatedSkinEntry* FindReplicatedEntryByDefinition(const USkinDefinition* SkinDefinition) const;

public:
	UPROPERTY(Transient, BlueprintReadOnly, Category = "!Inventory|Filter")
	TArray<FGameplayTag> FilterTypeTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "!Inventory")
	FSkinList AllSkinList;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "!Inventory")
	TMap<FGameplayTag, FSkinList> Map_Type_SkinList;

protected:
	UPROPERTY(Replicated)
	FReplicatedSkinList ReplicatedEntries;
};
