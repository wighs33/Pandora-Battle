#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/PrimaryAssetId.h"
#include "ContentDataSubsystem.generated.h"

class FContentLease;
struct FStreamableHandle;
class UPandoraDefinition;
class USkinDefinition;

DECLARE_LOG_CATEGORY_EXTERN(ContentDataSubsystemLog, Log, All);

UCLASS()
class LABPROJECT_API UContentDataSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Public API ------------------------------------------------------------------------------------------------------
	TSharedPtr<FStreamableHandle> PreloadPandoraDataAssetsAsync(FSimpleDelegate OnComplete = FSimpleDelegate());
	TSharedPtr<FStreamableHandle> PreloadSkinDataAssetsAsync(FSimpleDelegate OnComplete = FSimpleDelegate());

	/** Starts and retains the process-wide skill preload. Safe to call repeatedly. */
	void EnsureSkillDataAssetsPreload();
	bool IsSkillDataAssetsReady() const { return bSkillDataAssetsReady; }

	/** 반환된 lease가 살아 있는 동안 콘텐츠를 유지하며, 완료 콜백은 다음 ticker에서 전달한다. */
	TSharedPtr<FContentLease> AcquireContent(
		const TArray<FSoftObjectPath>& AssetPaths,
		FSimpleDelegate OnComplete = FSimpleDelegate());

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

	UFUNCTION(BlueprintPure, Category = "!ContentData|Pandora")
	UPandoraDefinition* GetPandoraDefinitionByName(FName PandoraName) const;

	UFUNCTION(BlueprintPure, Category = "!ContentData|Skin")
	USkinDefinition* GetSkinDefinitionByName(FName SkinName) const;

	FPrimaryAssetId GetSkinDefinitionIdByName(FName SkinName) const;

	void GetSkinDefinitionIds(TArray<FPrimaryAssetId>& OutAssetIds) const;
	// Profile bootstrap needs these entitlements immediately, including before UI preload.
	void GetDefaultSkinDefinitionIds(TArray<FPrimaryAssetId>& OutAssetIds) const;

	void GetLoadedPandoraDefinitionsByName(TMap<FName, TObjectPtr<UPandoraDefinition>>& OutAssets) const;
	void GetLoadedSkinDefinitionsByName(TMap<FName, TObjectPtr<USkinDefinition>>& OutAssets) const;

private:
	TSharedPtr<FStreamableHandle> PreloadSkillDataAssetsAsync(FSimpleDelegate OnComplete);
	FPrimaryAssetId GetPandoraDefinitionIdByName(FName PandoraName) const;
	void GetPandoraDefinitionIds(TArray<FPrimaryAssetId>& OutAssetIds) const;

	// Event Handlers --------------------------------------------------------------------------------------------------
	void HandleSkillDataAssetsPreloaded();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void BuildPrimaryAssetIndexes();
	void BuildPrimaryAssetIndex(
		FPrimaryAssetType AssetType,
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
	bool AreSkillDataAssetsLoaded() const;
	UObject* LoadPrimaryAssetOnDemand(const FPrimaryAssetId& AssetId) const;

	template <typename AssetType>
	void GatherLoadedAssets(
		const TMap<FName, FPrimaryAssetId>& AssetIdsByName,
		TMap<FName, TObjectPtr<AssetType>>& OutAssets) const;

private:
	TMap<FName, FPrimaryAssetId> PandoraDefinitionIdsByName;
	TMap<FName, FPrimaryAssetId> SkinDefinitionIdsByName;

	TMap<FName, FSoftObjectPath> PandoraDefinitionPathsByName;
	TMap<FName, FSoftObjectPath> SkinDefinitionPathsByName;

	TSet<FName> InvalidPandoraNames;
	TSet<FName> InvalidSkinNames;

	TSharedPtr<FStreamableHandle> SkillDataAssetsPreloadHandle;
	bool bSkillDataAssetsPreloadPending = false;
	bool bSkillDataAssetsReady = false;

	mutable TMap<FPrimaryAssetId, TSharedPtr<FStreamableHandle>>
		PendingOnDemandLoadHandles;
};
