#include "Component/Item/InventoryComponent.h"

#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Definition/Item/ItemDefinition.h"
#include "Item/ItemInstance.h"

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

	for (auto HandleIt = WeaponLoadoutPresentationHandles.CreateIterator(); HandleIt; ++HandleIt)
	{
		if (DesiredItemDefinitions.Contains(HandleIt.Key()))
		{
			continue;
		}

		if (HandleIt.Value().IsValid())
		{
			HandleIt.Value()->ReleaseHandle();
		}
		HandleIt.RemoveCurrent();
	}

	UAssetManager& AssetManager = UAssetManager::Get();
	const FName PresentationBundleName = UItemDefinition::GetWeaponPresentationBundleName();
	for (const TPair<FPrimaryAssetId, const UItemDefinition*>& DesiredPair
		: DesiredItemDefinitions)
	{
		const FPrimaryAssetId& AssetId = DesiredPair.Key;
		if (WeaponLoadoutPresentationHandles.Contains(AssetId))
		{
			continue;
		}

		// AssetManager bundle state is global; each inventory retains its own
		// handle for the lifetime of its loadout references.
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
			WeaponLoadoutPresentationHandles.Add(AssetId, nullptr);
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

		WeaponLoadoutPresentationHandles.Add(AssetId, MoveTemp(LoadHandle));
	}
}

void UInventoryComponent::ReleaseWeaponLoadoutPresentationAssets()
{
	for (TPair<FPrimaryAssetId, TSharedPtr<FStreamableHandle>>& HandlePair :
		WeaponLoadoutPresentationHandles)
	{
		if (HandlePair.Value.IsValid())
		{
			HandlePair.Value->ReleaseHandle();
		}
	}
	WeaponLoadoutPresentationHandles.Reset();
}
