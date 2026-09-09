#include "Component/Item/InventoryComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Definition/Common/ProjectTagConfig.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/Actor.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "Definition/Item/ItemDefinition.h"
#include "Item/ItemInstance.h"
#include "Mode/PdPlayerState.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"

bool UInventoryComponent::HasPendingItemLoads()
{
	CleanupCompletedItemLoadHandles();
	return PendingItemLoadRequestCount > 0
		|| !PendingItemLoadHandles.IsEmpty();
}

void UInventoryComponent::CleanupCompletedItemLoadHandles()
{
	PendingItemLoadHandles.RemoveAll(
		[](const TSharedPtr<FStreamableHandle>& PendingHandle)
		{
			return !PendingHandle.IsValid() || PendingHandle->HasLoadCompleted();
		});
}

void UInventoryComponent::CompletePendingItemLoadRequest(
	const uint64 RequestGeneration)
{
	if (RequestGeneration != ItemLoadGeneration)
	{
		return;
	}

	if (ensure(PendingItemLoadRequestCount > 0))
	{
		--PendingItemLoadRequestCount;
	}
}

void UInventoryComponent::CancelPendingItemLoads()
{
	// CancelHandle cannot retract a completion delegate that is already queued.
	// Advancing the generation makes every callback from the old batch a no-op.
	++ItemLoadGeneration;
	PendingItemLoadRequestCount = 0;

	for (const TSharedPtr<FStreamableHandle>& PendingHandle : PendingItemLoadHandles)
	{
		if (PendingHandle.IsValid() && !PendingHandle->HasLoadCompleted())
		{
			PendingHandle->CancelHandle();
		}
	}

	PendingItemLoadHandles.Reset();
}

void UInventoryComponent::RefreshWeaponLoadoutPresentationAssets()
{
	EnsureWeaponLoadoutSlotCount();

	TMap<FPrimaryAssetId, const UItemDefinition*> DesiredItemDefinitions;
	for (const FGuid& WeaponItemId : WeaponIdsByLoadoutSlot)
	{
		const UItemInstance* WeaponInstance = FindItemInstanceById(WeaponItemId);
		const UItemDefinition* ItemDefinition =
			IsValid(WeaponInstance) ? WeaponInstance->ItemDefinition.Get() : nullptr;
		if (!IsValid(ItemDefinition))
		{
			continue;
		}

		const FPrimaryAssetId AssetId = ItemDefinition->GetPrimaryAssetId();
		if (AssetId.IsValid())
		{
			DesiredItemDefinitions.Add(AssetId, ItemDefinition);
		}
	}

	for (auto HandleIt = PandoraWeaponPresentationLoadHandles.CreateIterator(); HandleIt; ++HandleIt)
	{
		if (DesiredItemDefinitions.Contains(HandleIt.Key()))
		{
			continue;
		}

		for (const TSharedPtr<FStreamableHandle>& LoadHandle : HandleIt.Value())
		{
			if (LoadHandle.IsValid())
			{
				LoadHandle->ReleaseHandle();
			}
		}
		HandleIt.RemoveCurrent();
	}

	UAssetManager& AssetManager = UAssetManager::Get();
	const FName PresentationBundleName = UItemDefinition::GetWeaponPresentationBundleName();
	for (const TPair<FPrimaryAssetId, const UItemDefinition*>& DesiredPair
		: DesiredItemDefinitions)
	{
		const FPrimaryAssetId& AssetId = DesiredPair.Key;
		if (PandoraWeaponPresentationLoadHandles.Contains(AssetId))
		{
			continue;
		}

		// Resolve the serialized bundle to concrete soft paths, then own a
		// streamable handle per inventory. UAssetManager bundle state is global
		// and LoadPrimaryAsset legitimately returns no handle when that state is
		// already active; treating that no-op as a failure produced the
		// DA_Greatsword error and also made per-player release semantics unclear.
		TArray<FSoftObjectPath> PresentationAssetPaths;
		const FAssetBundleEntry BundleEntry =
			AssetManager.GetAssetBundleEntry(
				AssetId,
				PresentationBundleName);
		if (BundleEntry.IsValid())
		{
			for (const FTopLevelAssetPath& AssetPath : BundleEntry.AssetPaths)
			{
				PresentationAssetPaths.AddUnique(FSoftObjectPath(AssetPath));
			}
		}

		// Merge paths from the loaded definition as an editor-safe fallback for
		// assets that predate the serialized bundle metadata. The metadata still
		// remains responsible for including these references in cooked builds.
		if (IsValid(DesiredPair.Value))
		{
			TArray<FSoftObjectPath> DefinitionPaths;
			DesiredPair.Value->GetWeaponPresentationAssetPaths(DefinitionPaths);
			for (const FSoftObjectPath& AssetPath : DefinitionPaths)
			{
				if (!AssetPath.IsNull())
				{
					PresentationAssetPaths.AddUnique(AssetPath);
				}
			}
		}

		PresentationAssetPaths.RemoveAll(
			[](const FSoftObjectPath& AssetPath)
			{
				return AssetPath.IsNull();
			});

		// A weapon definition with no presentation references has nothing to
		// preload. Record an empty sentinel so subsequent refreshes stay cheap.
		if (PresentationAssetPaths.IsEmpty())
		{
			PandoraWeaponPresentationLoadHandles.Add(AssetId, {});
			continue;
		}

		TSharedPtr<FStreamableHandle> LoadHandle =
			AssetManager.GetStreamableManager().RequestAsyncLoad(
				PresentationAssetPaths);
		if (!LoadHandle.IsValid())
		{
			UE_LOG(
				InventoryComponentLog,
				Error,
				TEXT("Failed to start weapon presentation preload for loadout item '%s'."),
				*AssetId.ToString());
			continue;
		}

		const TWeakPtr<FStreamableHandle> WeakLoadHandle = LoadHandle;
		LoadHandle->BindCompleteDelegate(
			FStreamableDelegate::CreateWeakLambda(
				this,
				[AssetId, WeakLoadHandle]()
				{
					const TSharedPtr<FStreamableHandle> CompletedHandle =
						WeakLoadHandle.Pin();
					if (CompletedHandle.IsValid() && CompletedHandle->HasError())
					{
						UE_LOG(
							InventoryComponentLog,
							Error,
							TEXT("Weapon presentation assets failed to load for item '%s'."),
							*AssetId.ToString());
					}
				}));

		TArray<TSharedPtr<FStreamableHandle>> LoadHandles;
		LoadHandles.Add(MoveTemp(LoadHandle));
		PandoraWeaponPresentationLoadHandles.Add(
			AssetId,
			MoveTemp(LoadHandles));
	}
}

void UInventoryComponent::ReleaseWeaponLoadoutPresentationAssets()
{
	for (TPair<FPrimaryAssetId, TArray<TSharedPtr<FStreamableHandle>>>& HandlePair :
		PandoraWeaponPresentationLoadHandles)
	{
		for (const TSharedPtr<FStreamableHandle>& LoadHandle : HandlePair.Value)
		{
			if (LoadHandle.IsValid())
			{
				LoadHandle->ReleaseHandle();
			}
		}
	}
	PandoraWeaponPresentationLoadHandles.Reset();
}
