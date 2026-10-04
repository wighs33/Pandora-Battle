#include "Component/Item/InventoryComponent.h"

#include "Data/ContentDataSubsystem.h"
#include "Data/ContentLease.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "Definition/Item/ItemDefinition.h"
#include "Item/ItemInstance.h"

namespace
{
	/** 무기 하나의 표현 애셋 경로. 쿡 빌드는 번들 메타데이터가, 메타데이터가 없던 옛 애셋은 불러온 정의의 경로가 채운다. */
	void CollectWeaponPresentationPaths(const FPrimaryAssetId& AssetId, const UItemDefinition* ItemDefinition,
		TArray<FSoftObjectPath>& OutPaths)
	{
		// 애셋 매니저의 번들 상태는 전역이라, 인벤토리마다 로드아웃이 참조하는 동안 자기 lease를 따로 쥔다.
		const FAssetBundleEntry BundleEntry =
			UAssetManager::Get().GetAssetBundleEntry(AssetId, UItemDefinition::GetWeaponPresentationBundleName());
		if (BundleEntry.IsValid())
		{
			for (const FTopLevelAssetPath& AssetPath : BundleEntry.AssetPaths)
			{
				OutPaths.AddUnique(FSoftObjectPath(AssetPath));
			}
		}

		// 번들 메타데이터보다 먼저 만든 애셋을 위해, 에디터에서는 불러온 정의의 경로도 합친다.
		// 쿡 빌드에서 이 참조를 담는 일은 여전히 메타데이터가 맡는다.
		if (IsValid(ItemDefinition))
		{
			TArray<FSoftObjectPath> DefinitionPaths;
			ItemDefinition->GetWeaponPresentationAssetPaths(DefinitionPaths);
			for (const FSoftObjectPath& AssetPath : DefinitionPaths)
			{
				if (!AssetPath.IsNull())
				{
					OutPaths.AddUnique(AssetPath);
				}
			}
		}

		OutPaths.RemoveAll([](const FSoftObjectPath& AssetPath)
		{
			return AssetPath.IsNull();
		});
	}
}

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
		if (PendingItemLoadRequestCount == 0)
		{
			ItemLoadsFinished.Broadcast();
		}
	}
}

void UInventoryComponent::CancelPendingItemLoads()
{
	// CancelHandle cannot retract a completion delegate that is already queued.
	// Advancing the generation makes every callback from the old batch a no-op.
	const bool bHadPendingLoads = PendingItemLoadRequestCount > 0 || !PendingItemLoadHandles.IsEmpty();
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
	if (bHadPendingLoads)
	{
		ItemLoadsFinished.Broadcast();
	}
}

void UInventoryComponent::RefreshWeaponLoadoutPresentationAssets()
{
	EnsureWeaponLoadoutSlotCount();

	TMap<FPrimaryAssetId, const UItemDefinition*> DesiredItemDefinitions;
	CollectLoadoutWeaponDefinitions(DesiredItemDefinitions);
	for (auto LeaseIt = WeaponLoadoutPresentationLeases.CreateIterator(); LeaseIt; ++LeaseIt)
	{
		if (!DesiredItemDefinitions.Contains(LeaseIt.Key()))
		{
			LeaseIt.RemoveCurrent();
		}
	}

	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UContentDataSubsystem* ContentSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
	if (!ContentSubsystem)
	{
		return;
	}

	for (const TPair<FPrimaryAssetId, const UItemDefinition*>& DesiredPair : DesiredItemDefinitions)
	{
		const FPrimaryAssetId& AssetId = DesiredPair.Key;
		if (WeaponLoadoutPresentationLeases.Contains(AssetId))
		{
			continue;
		}

		TArray<FSoftObjectPath> PresentationAssetPaths;
		CollectWeaponPresentationPaths(AssetId, DesiredPair.Value, PresentationAssetPaths);
		// 표현 애셋이 없는 무기는 미리 불러올 것이 없다. 빈 표시를 남겨 다음 갱신에서 다시 찾지 않는다.
		if (PresentationAssetPaths.IsEmpty())
		{
			WeaponLoadoutPresentationLeases.Add(AssetId, nullptr);
			continue;
		}

		// 시작하지 못한 lease는 보관하지 않아 다음 갱신에서 다시 시도한다.
		TSharedPtr<FContentLease> PresentationLease =
			ContentSubsystem->AcquireContent(PresentationAssetPaths);
		if (PresentationLease->HasFailed())
		{
			UE_LOG(
				InventoryComponentLog,
				Error,
				TEXT("Failed to start weapon presentation preload for loadout item '%s'."),
				*AssetId.ToString());
			continue;
		}

		WeaponLoadoutPresentationLeases.Add(AssetId, MoveTemp(PresentationLease));
	}
}

void UInventoryComponent::CollectLoadoutWeaponDefinitions(TMap<FPrimaryAssetId, const UItemDefinition*>& OutDefinitions) const
{
	for (const FGuid& WeaponItemId : WeaponIdsByLoadoutSlot)
	{
		const UItemInstance* WeaponInstance = FindItemInstanceById(WeaponItemId);
		const UItemDefinition* ItemDefinition = IsValid(WeaponInstance) ? WeaponInstance->ItemDefinition.Get() : nullptr;
		if (!IsValid(ItemDefinition))
		{
			continue;
		}

		const FPrimaryAssetId AssetId = ItemDefinition->GetPrimaryAssetId();
		if (AssetId.IsValid())
		{
			OutDefinitions.Add(AssetId, ItemDefinition);
		}
	}
}

void UInventoryComponent::ReleaseWeaponLoadoutPresentationAssets()
{
	WeaponLoadoutPresentationLeases.Reset();
}
