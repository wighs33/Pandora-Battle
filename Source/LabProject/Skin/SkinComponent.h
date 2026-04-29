// Fill out your copyright notice in the Description page of Project Settings.

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

DECLARE_LOG_CATEGORY_EXTERN(SkinComponentLog, Log, All);

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

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LABPROJECT_API USkinComponent : public UPlayerStateComponent
{
	GENERATED_BODY()

	friend struct FReplicatedSkinEntry;

public:
	USkinComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "!Inventory", meta = (DisplayName = "Make&AddSkins"))
	void MakeAndAddSkins(const TArray<FPrimaryAssetId>& SkinDefinitions);

	UFUNCTION(BlueprintCallable, Category = "!Inventory")
	void AddSkinsByPrimaryAssetIds(const TArray<FPrimaryAssetId>& SkinDefinitions);

	UFUNCTION(BlueprintCallable, Category = "!Inventory")
	void FilterSkin(USkinInstance* SkinInstance);

	UFUNCTION(BlueprintCallable, Category = "!Inventory")
	void AddValueToMap(FGameplayTag TypeTag, USkinInstance* SkinInstance);

protected:
	void InitializeReplicatedEntriesFromRuntimeSkins();
	void RebuildRuntimeSkinsFromReplicatedEntries();
	void RebuildFilteredSkinMap();
	void HandleReplicatedEntryAddedOrChanged(const FReplicatedSkinEntry& Entry);
	void HandleReplicatedEntryRemoved(const USkinDefinition* SkinDefinition);
	void AddReplicatedSkin(USkinInstance* SkinInstance);
	USkinInstance* FindSkinInstanceByDefinition(const USkinDefinition* SkinDefinition) const;
	int32 FindReplicatedEntryIndexByDefinition(const USkinDefinition* SkinDefinition) const;
	FReplicatedSkinEntry* FindReplicatedEntryByDefinition(const USkinDefinition* SkinDefinition);
	const FReplicatedSkinEntry* FindReplicatedEntryByDefinition(const USkinDefinition* SkinDefinition) const;

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Inventory|Filter", meta = (Categories = "Skin"))
	TArray<FGameplayTag> FilterTypeTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "!Inventory")
	FSkinList AllSkinList;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "!Inventory")
	TMap<FGameplayTag, FSkinList> Map_Type_SkinList;

protected:
	UPROPERTY(Replicated)
	FReplicatedSkinList ReplicatedEntries;
};
