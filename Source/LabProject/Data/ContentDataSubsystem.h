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

	/** 프로세스 전체에서 쓰는 스킬 미리 로드를 시작하고 유지한다. 여러 번 불러도 된다. */
	void EnsureSkillDataAssetsPreload();
	bool IsSkillDataAssetsLoading() const { return bSkillDataAssetsPreloadPending; }

	/**
	 * 반환된 lease가 살아 있는 동안 콘텐츠를 유지한다. 완료 콜백은 로드가 끝나는 프레임에, 빈 목록이면 다음 ticker에서 전달한다.
	 * GameInstance가 없는 에디터 월드(레벨에 놓인 액터, 블루프린트 미리보기)에서도 쓸 수 있다.
	 */
	static TSharedPtr<FContentLease> AcquireContent(const TArray<FSoftObjectPath>& AssetPaths,
		FSimpleDelegate OnComplete = FSimpleDelegate());

	/**
	 * 소프트 참조를 게임 스레드를 막지 않고 미리 로드한다.
	 * 돌려받은 핸들은 호출한 쪽이 소유하며, 콘텐츠가 더 필요 없으면 해제한다.
	 */
	static TSharedPtr<FStreamableHandle> PreloadSoftObjectPathsAsync(const TArray<FSoftObjectPath>& AssetPaths,
		FSimpleDelegate OnComplete = FSimpleDelegate());

	/**
	 * 주어진 Primary Asset의 런타임 번들을 막지 않고 미리 로드한다.
	 * 정해진 데이터 애셋 묶음을 소유하는 시스템이 공통으로 쓰는 진입점이다.
	 */
	TSharedPtr<FStreamableHandle> PreloadPrimaryAssetsAsync(const TArray<FPrimaryAssetId>& AssetIds,
		FSimpleDelegate OnComplete = FSimpleDelegate());

	UFUNCTION(BlueprintPure, Category = "!ContentData|Pandora")
	UPandoraDefinition* GetPandoraDefinitionByName(FName PandoraName) const;

	UFUNCTION(BlueprintPure, Category = "!ContentData|Skin")
	USkinDefinition* GetSkinDefinitionByName(FName SkinName) const;

	FPrimaryAssetId GetSkinDefinitionIdByName(FName SkinName) const;

	void GetSkinDefinitionIds(TArray<FPrimaryAssetId>& OutAssetIds) const;
	// 프로필 초기화가 UI 미리 로드보다 먼저 이 기본 보유 목록을 바로 쓴다.
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
	void BuildPrimaryAssetIndex(FPrimaryAssetType AssetType, TMap<FName, FPrimaryAssetId>& OutAssetIdsByName,
		TMap<FName, FSoftObjectPath>& OutAssetPathsByName, TSet<FName>& OutInvalidNames);
	void AddIndexedName(FPrimaryAssetType AssetType, FName LookupName, const FPrimaryAssetId& AssetId,
		const FSoftObjectPath& AssetPath, TMap<FName, FPrimaryAssetId>& AssetIdsByName,
		TMap<FName, FSoftObjectPath>& AssetPathsByName, TSet<FName>& InvalidNames);

	TArray<FName> GetRuntimeBundles() const;
	bool AreSkillDataAssetsLoaded() const;
	UObject* LoadPrimaryAssetOnDemand(const FPrimaryAssetId& AssetId) const;

	template <typename AssetType>
	void GatherLoadedAssets(const TMap<FName, FPrimaryAssetId>& AssetIdsByName,
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
