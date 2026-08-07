#pragma once

#include "CoreMinimal.h"
#include "Component/AbilitySystem/PandoraTreeComponent.h"
#include "Definition/AbilitySystem/SkillTypes.h"
#include "Engine/StreamableManager.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/PrimaryAssetId.h"
#include "ContentDataSubsystem.generated.h"

class UPandoraDefinition;
class USkinDefinition;

DECLARE_LOG_CATEGORY_EXTERN(ContentDataSubsystemLog, Log, All);

UCLASS()
class LABPROJECT_API UContentDataSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * Legacy compatibility entry points. They now start an asynchronous Primary Asset load.
	 * New native code should keep the handle returned by the matching Async method.
	 */
	UFUNCTION(BlueprintCallable, Category = "!ContentData|Skill",
		meta = (DeprecatedFunction, DeprecationMessage = "Use LoadSkillDataAssetsAsync and wait for completion."))
	void LoadSkillDataAssetsToMemory();

	UFUNCTION(BlueprintCallable, Category = "!ContentData|Pandora",
		meta = (DeprecatedFunction, DeprecationMessage = "Use LoadPandoraDataAssetsAsync and wait for completion."))
	void LoadPandoraDataAssetsToMemory();

	UFUNCTION(BlueprintCallable, Category = "!ContentData|Skin",
		meta = (DeprecatedFunction, DeprecationMessage = "Use LoadSkinDataAssetsAsync and wait for completion."))
	void LoadSkinDataAssetsToMemory();

	/** Loads all definitions of a type without blocking. Call ReleaseHandle on preload handles when the consumer closes. */
	TSharedPtr<FStreamableHandle> LoadSkillDataAssetsAsync(FSimpleDelegate OnComplete = FSimpleDelegate());
	TSharedPtr<FStreamableHandle> LoadPandoraDataAssetsAsync(FSimpleDelegate OnComplete = FSimpleDelegate());
	TSharedPtr<FStreamableHandle> LoadSkinDataAssetsAsync(FSimpleDelegate OnComplete = FSimpleDelegate());

	TSharedPtr<FStreamableHandle> PreloadSkillDataAssetsAsync(FSimpleDelegate OnComplete = FSimpleDelegate());
	TSharedPtr<FStreamableHandle> PreloadPandoraDataAssetsAsync(FSimpleDelegate OnComplete = FSimpleDelegate());
	TSharedPtr<FStreamableHandle> PreloadSkinDataAssetsAsync(FSimpleDelegate OnComplete = FSimpleDelegate());

	/** Starts and retains the process-wide skill preload. Safe to call repeatedly. */
	void EnsureSkillDataAssetsPreload();
	bool IsSkillDataAssetsReady() const { return bSkillDataAssetsReady; }

	/**
	 * Preloads arbitrary soft references without blocking the game thread.
	 * The caller owns the returned handle and should release it when the content is no longer needed.
	 */
	TSharedPtr<FStreamableHandle> PreloadSoftObjectPathsAsync(
		const TArray<FSoftObjectPath>& AssetPaths,
		FSimpleDelegate OnComplete = FSimpleDelegate());

	/**
	 * Preloads the runtime bundles for the supplied Primary Assets without blocking.
	 * This is the common entry point for systems that own a known set of data assets.
	 */
	TSharedPtr<FStreamableHandle> PreloadPrimaryAssetsAsync(
		const TArray<FPrimaryAssetId>& AssetIds,
		FSimpleDelegate OnComplete = FSimpleDelegate());

	/**
	 * Compatibility lookups never block. They return an already loaded definition, or start an
	 * asynchronous on-demand request and return null until a later call.
	 * Prefer the explicit Async APIs when the caller must continue immediately after completion.
	 */
	UFUNCTION(BlueprintPure, Category = "!ContentData|Skill")
	USkillDefinition* GetSkillDataAssetByName(FName SkillName) const;

	UFUNCTION(BlueprintPure, Category = "!ContentData|Pandora")
	UPandoraDefinition* GetPandoraDefinitionByName(FName PandoraName) const;

	UFUNCTION(BlueprintPure, Category = "!ContentData|Skin")
	USkinDefinition* GetSkinDefinitionByName(FName SkinName) const;

	FPrimaryAssetId GetSkillDataAssetIdByName(FName SkillName) const;
	FPrimaryAssetId GetPandoraDefinitionIdByName(FName PandoraName) const;
	FPrimaryAssetId GetSkinDefinitionIdByName(FName SkinName) const;

	void GetSkillDataAssetIds(TArray<FPrimaryAssetId>& OutAssetIds) const;
	void GetPandoraDefinitionIds(TArray<FPrimaryAssetId>& OutAssetIds) const;
	void GetSkinDefinitionIds(TArray<FPrimaryAssetId>& OutAssetIds) const;

	void GetLoadedSkillDataAssetsByName(TMap<FName, TObjectPtr<USkillDefinition>>& OutAssets) const;
	void GetLoadedPandoraDefinitionsByName(TMap<FName, TObjectPtr<UPandoraDefinition>>& OutAssets) const;
	void GetLoadedSkinDefinitionsByName(TMap<FName, TObjectPtr<USkinDefinition>>& OutAssets) const;

	void BuildGrantedPandorasFromNames(
		const TMap<FName, int32>& GrantedPandorasByName,
		TArray<FGrantedPandora>& OutGrantedPandoras) const;
	void BuildDefaultUnlockedPandoras(
		TArray<FName>& OutOwnedPandoraNames,
		TArray<FPrimaryAssetId>* OutPandoraDefinitionIds = nullptr) const;
	void BuildGrantedSkinDefinitionsFromNames(
		const TMap<FName, int32>& GrantedSkinsByName,
		TArray<USkinDefinition*>& OutSkinDefinitions) const;

private:
	void BuildPrimaryAssetIndexes();
	void BuildPrimaryAssetIndex(
		FPrimaryAssetType AssetType,
		FName OptionalLogicalNameTag,
		TMap<FName, FPrimaryAssetId>& OutAssetIdsByName,
		TMap<FName, FSoftObjectPath>& OutAssetPathsByName,
		TSet<FName>& OutInvalidNames);
	void AddIndexedName(
		FPrimaryAssetType AssetType,
		FName LookupName,
		const FPrimaryAssetId& AssetId,
		const FSoftObjectPath& AssetPath,
		TMap<FName, FPrimaryAssetId>& AssetIdsByName,
		TMap<FName, FSoftObjectPath>& AssetPathsByName,
		TSet<FName>& InvalidNames);

	TArray<FName> GetRuntimeBundles() const;
	TSharedPtr<FStreamableHandle> LoadPrimaryAssetTypeAsync(
		const TArray<FPrimaryAssetId>& AssetIds,
		FSimpleDelegate OnComplete,
		bool bPreload);
	void HandleSkillDataAssetsPreloaded();
	bool AreSkillDataAssetsLoaded() const;
	UObject* LoadPrimaryAssetOnDemand(const FPrimaryAssetId& AssetId) const;

	template <typename AssetType>
	void GatherLoadedAssets(
		const TMap<FName, FPrimaryAssetId>& AssetIdsByName,
		TMap<FName, TObjectPtr<AssetType>>& OutAssets) const;

	TMap<FName, FPrimaryAssetId> SkillDataAssetIdsByName;
	TMap<FName, FPrimaryAssetId> PandoraDefinitionIdsByName;
	TMap<FName, FPrimaryAssetId> SkinDefinitionIdsByName;

	TMap<FName, FSoftObjectPath> SkillDataAssetPathsByName;
	TMap<FName, FSoftObjectPath> PandoraDefinitionPathsByName;
	TMap<FName, FSoftObjectPath> SkinDefinitionPathsByName;

	TSet<FName> InvalidSkillNames;
	TSet<FName> InvalidPandoraNames;
	TSet<FName> InvalidSkinNames;

	TSharedPtr<FStreamableHandle> SkillDataAssetsPreloadHandle;
	bool bSkillDataAssetsPreloadPending = false;
	bool bSkillDataAssetsReady = false;

	mutable TMap<FPrimaryAssetId, TSharedPtr<FStreamableHandle>>
		PendingOnDemandLoadHandles;
};
