#include "Data/ContentDataSubsystem.h"

#include "AssetRegistry/AssetData.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "Definition/Skin/SkinDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "Pandora/PandoraDefaultUnlockPolicy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ContentDataSubsystem)

DEFINE_LOG_CATEGORY(ContentDataSubsystemLog);

namespace
{
	const FPrimaryAssetType SkillAssetType(TEXT("Skill"));
	const FPrimaryAssetType PandoraAssetType(TEXT("PandoraDefinition"));
	const FPrimaryAssetType SkinAssetType(TEXT("SkinDefinition"));
	const FName ClientBundle(TEXT("Client"));
	const FName ServerBundle(TEXT("Server"));

	void GatherUniqueSortedAssetIds(
		const TMap<FName, FPrimaryAssetId>& AssetIdsByName,
		TArray<FPrimaryAssetId>& OutAssetIds)
	{
		TSet<FPrimaryAssetId> UniqueAssetIds;
		for (const TPair<FName, FPrimaryAssetId>& AssetPair : AssetIdsByName)
		{
			if (AssetPair.Value.IsValid())
			{
				UniqueAssetIds.Add(AssetPair.Value);
			}
		}

		OutAssetIds.Reset(UniqueAssetIds.Num());
		for (const FPrimaryAssetId& AssetId : UniqueAssetIds)
		{
			OutAssetIds.Add(AssetId);
		}
		OutAssetIds.Sort([](const FPrimaryAssetId& Left, const FPrimaryAssetId& Right)
		{
			return Left.ToString() < Right.ToString();
		});
	}
}

void UContentDataSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	BuildPrimaryAssetIndexes();
}

void UContentDataSubsystem::Deinitialize()
{
	for (TPair<FPrimaryAssetId, TSharedPtr<FStreamableHandle>>& LoadPair
		: PendingOnDemandLoadHandles)
	{
		if (LoadPair.Value.IsValid())
		{
			LoadPair.Value->CancelHandle();
			LoadPair.Value->ReleaseHandle();
		}
	}
	PendingOnDemandLoadHandles.Reset();

	SkillDataAssetIdsByName.Reset();
	PandoraDefinitionIdsByName.Reset();
	SkinDefinitionIdsByName.Reset();
	SkillDataAssetPathsByName.Reset();
	PandoraDefinitionPathsByName.Reset();
	SkinDefinitionPathsByName.Reset();
	InvalidSkillNames.Reset();
	InvalidPandoraNames.Reset();
	InvalidSkinNames.Reset();

	Super::Deinitialize();
}

void UContentDataSubsystem::LoadSkillDataAssetsToMemory()
{
	LoadSkillDataAssetsAsync();
}

void UContentDataSubsystem::LoadPandoraDataAssetsToMemory()
{
	LoadPandoraDataAssetsAsync();
}

void UContentDataSubsystem::LoadSkinDataAssetsToMemory()
{
	LoadSkinDataAssetsAsync();
}

TSharedPtr<FStreamableHandle> UContentDataSubsystem::LoadSkillDataAssetsAsync(FSimpleDelegate OnComplete)
{
	TArray<FPrimaryAssetId> AssetIds;
	GetSkillDataAssetIds(AssetIds);
	return LoadPrimaryAssetTypeAsync(AssetIds, MoveTemp(OnComplete), false);
}

TSharedPtr<FStreamableHandle> UContentDataSubsystem::LoadPandoraDataAssetsAsync(FSimpleDelegate OnComplete)
{
	TArray<FPrimaryAssetId> AssetIds;
	GetPandoraDefinitionIds(AssetIds);
	return LoadPrimaryAssetTypeAsync(AssetIds, MoveTemp(OnComplete), false);
}

TSharedPtr<FStreamableHandle> UContentDataSubsystem::LoadSkinDataAssetsAsync(FSimpleDelegate OnComplete)
{
	TArray<FPrimaryAssetId> AssetIds;
	GetSkinDefinitionIds(AssetIds);
	return LoadPrimaryAssetTypeAsync(AssetIds, MoveTemp(OnComplete), false);
}

TSharedPtr<FStreamableHandle> UContentDataSubsystem::PreloadPandoraDataAssetsAsync(FSimpleDelegate OnComplete)
{
	TArray<FPrimaryAssetId> AssetIds;
	GetPandoraDefinitionIds(AssetIds);
	return LoadPrimaryAssetTypeAsync(AssetIds, MoveTemp(OnComplete), true);
}

TSharedPtr<FStreamableHandle> UContentDataSubsystem::PreloadSkinDataAssetsAsync(FSimpleDelegate OnComplete)
{
	TArray<FPrimaryAssetId> AssetIds;
	GetSkinDefinitionIds(AssetIds);
	return LoadPrimaryAssetTypeAsync(AssetIds, MoveTemp(OnComplete), true);
}

TSharedPtr<FStreamableHandle> UContentDataSubsystem::PreloadSoftObjectPathsAsync(
	const TArray<FSoftObjectPath>& AssetPaths,
	FSimpleDelegate OnComplete)
{
	TSet<FSoftObjectPath> UniquePaths;
	for (const FSoftObjectPath& AssetPath : AssetPaths)
	{
		if (AssetPath.IsValid() && !AssetPath.IsNull())
		{
			UniquePaths.Add(AssetPath);
		}
	}

	if (UniquePaths.IsEmpty())
	{
		OnComplete.ExecuteIfBound();
		return nullptr;
	}

	TArray<FSoftObjectPath> PathsToLoad = UniquePaths.Array();
	PathsToLoad.Sort([](const FSoftObjectPath& Left, const FSoftObjectPath& Right)
	{
		return Left.ToString() < Right.ToString();
	});

	const FSimpleDelegate CompletionDelegate = MoveTemp(OnComplete);
	TSharedPtr<FStreamableHandle> Handle =
		UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
			PathsToLoad,
			FStreamableDelegate::CreateLambda(
				[CompletionDelegate]()
				{
					CompletionDelegate.ExecuteIfBound();
				}));

	if (!Handle.IsValid())
	{
		UE_LOG(
			ContentDataSubsystemLog,
			Error,
			TEXT("Failed to start an asynchronous preload for %d soft object path(s)."),
			PathsToLoad.Num());
		CompletionDelegate.ExecuteIfBound();
	}

	return Handle;
}

TSharedPtr<FStreamableHandle> UContentDataSubsystem::PreloadPrimaryAssetsAsync(
	const TArray<FPrimaryAssetId>& AssetIds,
	FSimpleDelegate OnComplete)
{
	return LoadPrimaryAssetTypeAsync(AssetIds, MoveTemp(OnComplete), true);
}

USkillDefinition* UContentDataSubsystem::GetSkillDataAssetByName(const FName SkillName) const
{
	return Cast<USkillDefinition>(LoadPrimaryAssetOnDemand(GetSkillDataAssetIdByName(SkillName)));
}

UPandoraDefinition* UContentDataSubsystem::GetPandoraDefinitionByName(const FName PandoraName) const
{
	return Cast<UPandoraDefinition>(LoadPrimaryAssetOnDemand(GetPandoraDefinitionIdByName(PandoraName)));
}

USkinDefinition* UContentDataSubsystem::GetSkinDefinitionByName(const FName SkinName) const
{
	return Cast<USkinDefinition>(LoadPrimaryAssetOnDemand(GetSkinDefinitionIdByName(SkinName)));
}

FPrimaryAssetId UContentDataSubsystem::GetSkillDataAssetIdByName(const FName SkillName) const
{
	return InvalidSkillNames.Contains(SkillName) ? FPrimaryAssetId() : SkillDataAssetIdsByName.FindRef(SkillName);
}

FPrimaryAssetId UContentDataSubsystem::GetPandoraDefinitionIdByName(const FName PandoraName) const
{
	return InvalidPandoraNames.Contains(PandoraName) ? FPrimaryAssetId() : PandoraDefinitionIdsByName.FindRef(PandoraName);
}

FPrimaryAssetId UContentDataSubsystem::GetSkinDefinitionIdByName(const FName SkinName) const
{
	return InvalidSkinNames.Contains(SkinName) ? FPrimaryAssetId() : SkinDefinitionIdsByName.FindRef(SkinName);
}

void UContentDataSubsystem::GetSkillDataAssetIds(TArray<FPrimaryAssetId>& OutAssetIds) const
{
	GatherUniqueSortedAssetIds(SkillDataAssetIdsByName, OutAssetIds);
}

void UContentDataSubsystem::GetPandoraDefinitionIds(TArray<FPrimaryAssetId>& OutAssetIds) const
{
	GatherUniqueSortedAssetIds(PandoraDefinitionIdsByName, OutAssetIds);
}

void UContentDataSubsystem::GetSkinDefinitionIds(TArray<FPrimaryAssetId>& OutAssetIds) const
{
	GatherUniqueSortedAssetIds(SkinDefinitionIdsByName, OutAssetIds);
}

void UContentDataSubsystem::GetLoadedSkillDataAssetsByName(
	TMap<FName, TObjectPtr<USkillDefinition>>& OutAssets) const
{
	GatherLoadedAssets(SkillDataAssetIdsByName, OutAssets);
}

void UContentDataSubsystem::GetLoadedPandoraDefinitionsByName(
	TMap<FName, TObjectPtr<UPandoraDefinition>>& OutAssets) const
{
	GatherLoadedAssets(PandoraDefinitionIdsByName, OutAssets);
}

void UContentDataSubsystem::GetLoadedSkinDefinitionsByName(
	TMap<FName, TObjectPtr<USkinDefinition>>& OutAssets) const
{
	GatherLoadedAssets(SkinDefinitionIdsByName, OutAssets);
}

void UContentDataSubsystem::BuildGrantedPandorasFromNames(
	const TMap<FName, int32>& GrantedPandorasByName,
	TArray<FGrantedPandora>& OutGrantedPandoras) const
{
	OutGrantedPandoras.Reset();

	for (const TPair<FName, int32>& PandoraPair : GrantedPandorasByName)
	{
		if (PandoraDefaultUnlockPolicy::IsDefaultUnlockedPandoraName(PandoraPair.Key))
		{
			continue;
		}

		if (UPandoraDefinition* PandoraDefinition = GetPandoraDefinitionByName(PandoraPair.Key))
		{
			OutGrantedPandoras.AddUnique(
				FGrantedPandora(PandoraDefinition, FMath::Max(PandoraPair.Value, 1)));
		}
	}
}

void UContentDataSubsystem::BuildDefaultUnlockedPandoras(
	TArray<FName>& OutOwnedPandoraNames,
	TArray<FPrimaryAssetId>* OutPandoraDefinitionIds) const
{
	OutOwnedPandoraNames.Reset();
	if (OutPandoraDefinitionIds)
	{
		OutPandoraDefinitionIds->Reset();
	}

	TArray<FPrimaryAssetId> PandoraDefinitionIds;
	GetPandoraDefinitionIds(PandoraDefinitionIds);
	for (const FPrimaryAssetId& PandoraDefinitionId : PandoraDefinitionIds)
	{
		if (!PandoraDefaultUnlockPolicy::IsDefaultUnlockedPandoraName(
			PandoraDefinitionId.PrimaryAssetName))
		{
			continue;
		}

		OutOwnedPandoraNames.AddUnique(PandoraDefinitionId.PrimaryAssetName);
		if (OutPandoraDefinitionIds)
		{
			OutPandoraDefinitionIds->AddUnique(PandoraDefinitionId);
		}
	}
}

void UContentDataSubsystem::BuildGrantedSkinDefinitionsFromNames(
	const TMap<FName, int32>& GrantedSkinsByName,
	TArray<USkinDefinition*>& OutSkinDefinitions) const
{
	OutSkinDefinitions.Reset();

	for (const TPair<FName, int32>& SkinPair : GrantedSkinsByName)
	{
		if (USkinDefinition* SkinDefinition = GetSkinDefinitionByName(SkinPair.Key))
		{
			OutSkinDefinitions.AddUnique(SkinDefinition);
		}
	}
}

void UContentDataSubsystem::BuildPrimaryAssetIndexes()
{
	BuildPrimaryAssetIndex(
		SkillAssetType,
		GET_MEMBER_NAME_CHECKED(USkillDefinition, Name),
		SkillDataAssetIdsByName,
		SkillDataAssetPathsByName,
		InvalidSkillNames);
	BuildPrimaryAssetIndex(
		PandoraAssetType,
		NAME_None,
		PandoraDefinitionIdsByName,
		PandoraDefinitionPathsByName,
		InvalidPandoraNames);
	BuildPrimaryAssetIndex(
		SkinAssetType,
		NAME_None,
		SkinDefinitionIdsByName,
		SkinDefinitionPathsByName,
		InvalidSkinNames);
}

void UContentDataSubsystem::BuildPrimaryAssetIndex(
	const FPrimaryAssetType AssetType,
	const FName OptionalLogicalNameTag,
	TMap<FName, FPrimaryAssetId>& OutAssetIdsByName,
	TMap<FName, FSoftObjectPath>& OutAssetPathsByName,
	TSet<FName>& OutInvalidNames)
{
	OutAssetIdsByName.Reset();
	OutAssetPathsByName.Reset();
	OutInvalidNames.Reset();

	UAssetManager& AssetManager = UAssetManager::Get();
	TArray<FAssetData> AssetDataList;
	AssetManager.GetPrimaryAssetDataList(AssetType, AssetDataList);

	for (const FAssetData& AssetData : AssetDataList)
	{
		const FPrimaryAssetId AssetId = AssetManager.GetPrimaryAssetIdForData(AssetData);
		if (!AssetId.IsValid())
		{
			UE_LOG(
				ContentDataSubsystemLog,
				Error,
				TEXT("Primary asset '%s' has no valid PrimaryAssetId for type '%s'."),
				*AssetData.GetSoftObjectPath().ToString(),
				*AssetType.ToString());
			continue;
		}

		const FSoftObjectPath AssetPath = AssetData.GetSoftObjectPath();
		AddIndexedName(
			AssetType,
			AssetId.PrimaryAssetName,
			AssetId,
			AssetPath,
			OutAssetIdsByName,
			OutAssetPathsByName,
			OutInvalidNames);

		if (!OptionalLogicalNameTag.IsNone())
		{
			FName LogicalName = NAME_None;
			if (AssetData.GetTagValue(OptionalLogicalNameTag, LogicalName) && !LogicalName.IsNone())
			{
				AddIndexedName(
					AssetType,
					LogicalName,
					AssetId,
					AssetPath,
					OutAssetIdsByName,
					OutAssetPathsByName,
					OutInvalidNames);
			}
		}
	}
}

void UContentDataSubsystem::AddIndexedName(
	const FPrimaryAssetType AssetType,
	const FName LookupName,
	const FPrimaryAssetId& AssetId,
	const FSoftObjectPath& AssetPath,
	TMap<FName, FPrimaryAssetId>& AssetIdsByName,
	TMap<FName, FSoftObjectPath>& AssetPathsByName,
	TSet<FName>& InvalidNames)
{
	if (LookupName.IsNone() || InvalidNames.Contains(LookupName))
	{
		return;
	}

	const FPrimaryAssetId* ExistingId = AssetIdsByName.Find(LookupName);
	const FSoftObjectPath* ExistingPath = AssetPathsByName.Find(LookupName);
	if (ExistingId && ExistingPath && (*ExistingId != AssetId || *ExistingPath != AssetPath))
	{
		UE_LOG(
			ContentDataSubsystemLog,
			Error,
			TEXT("Duplicate %s content name '%s': '%s' and '%s'. The ambiguous name is disabled."),
			*AssetType.ToString(),
			*LookupName.ToString(),
			*ExistingPath->ToString(),
			*AssetPath.ToString());
		AssetIdsByName.Remove(LookupName);
		AssetPathsByName.Remove(LookupName);
		InvalidNames.Add(LookupName);
		return;
	}

	AssetIdsByName.Add(LookupName, AssetId);
	AssetPathsByName.Add(LookupName, AssetPath);
}

TArray<FName> UContentDataSubsystem::GetRuntimeBundles() const
{
	TArray<FName> Bundles;
	const UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	const ENetMode NetMode = World ? World->GetNetMode() : NM_Standalone;

	if (NetMode != NM_DedicatedServer)
	{
		Bundles.Add(ClientBundle);
	}
	if (NetMode != NM_Client)
	{
		Bundles.Add(ServerBundle);
	}
	return Bundles;
}

TSharedPtr<FStreamableHandle> UContentDataSubsystem::LoadPrimaryAssetTypeAsync(
	const TArray<FPrimaryAssetId>& AssetIds,
	FSimpleDelegate OnComplete,
	const bool bPreload)
{
	TSet<FPrimaryAssetId> UniqueAssetIds;
	for (const FPrimaryAssetId& AssetId : AssetIds)
	{
		if (AssetId.IsValid())
		{
			UniqueAssetIds.Add(AssetId);
		}
	}

	if (UniqueAssetIds.IsEmpty())
	{
		OnComplete.ExecuteIfBound();
		return nullptr;
	}

	TArray<FPrimaryAssetId> AssetIdsToLoad = UniqueAssetIds.Array();
	AssetIdsToLoad.Sort(
		[](const FPrimaryAssetId& Left, const FPrimaryAssetId& Right)
		{
			return Left.ToString() < Right.ToString();
		});

	UAssetManager& AssetManager = UAssetManager::Get();
	const FSimpleDelegate CompletionDelegate = MoveTemp(OnComplete);
	const FStreamableDelegate StreamableCompletion =
		FStreamableDelegate::CreateLambda(
			[CompletionDelegate]()
			{
				CompletionDelegate.ExecuteIfBound();
			});

	TSharedPtr<FStreamableHandle> Handle;
	if (bPreload)
	{
		Handle = AssetManager.PreloadPrimaryAssets(
			AssetIdsToLoad,
			GetRuntimeBundles(),
			false,
			StreamableCompletion);
	}
	else
	{
		Handle = AssetManager.LoadPrimaryAssets(
			AssetIdsToLoad,
			GetRuntimeBundles(),
			StreamableCompletion);
	}

	if (!Handle.IsValid())
	{
		UE_LOG(
			ContentDataSubsystemLog,
			Error,
			TEXT("Failed to start an asynchronous %s for %d primary asset(s)."),
			bPreload ? TEXT("preload") : TEXT("load"),
			AssetIdsToLoad.Num());
		CompletionDelegate.ExecuteIfBound();
	}

	return Handle;
}

UObject* UContentDataSubsystem::LoadPrimaryAssetOnDemand(const FPrimaryAssetId& AssetId) const
{
	if (!AssetId.IsValid())
	{
		return nullptr;
	}

	UAssetManager& AssetManager = UAssetManager::Get();
	if (UObject* LoadedAsset = AssetManager.GetPrimaryAssetObject(AssetId))
	{
		PendingOnDemandLoadHandles.Remove(AssetId);
		return LoadedAsset;
	}

	if (const TSharedPtr<FStreamableHandle>* ExistingHandle =
		PendingOnDemandLoadHandles.Find(AssetId);
		ExistingHandle && ExistingHandle->IsValid())
	{
		return nullptr;
	}

	TSharedPtr<FStreamableHandle> LoadHandle =
		AssetManager.LoadPrimaryAsset(AssetId, GetRuntimeBundles());
	if (!LoadHandle.IsValid())
	{
		UE_LOG(
			ContentDataSubsystemLog,
			Error,
			TEXT("Failed to start on-demand load for primary asset '%s'."),
			*AssetId.ToString());
		return nullptr;
	}

	PendingOnDemandLoadHandles.Add(AssetId, MoveTemp(LoadHandle));
	return nullptr;
}

template <typename AssetType>
void UContentDataSubsystem::GatherLoadedAssets(
	const TMap<FName, FPrimaryAssetId>& AssetIdsByName,
	TMap<FName, TObjectPtr<AssetType>>& OutAssets) const
{
	OutAssets.Reset();
	const UAssetManager& AssetManager = UAssetManager::Get();
	for (const TPair<FName, FPrimaryAssetId>& AssetPair : AssetIdsByName)
	{
		if (AssetType* Asset = Cast<AssetType>(AssetManager.GetPrimaryAssetObject(AssetPair.Value)))
		{
			OutAssets.Add(AssetPair.Key, Asset);
		}
	}
}
