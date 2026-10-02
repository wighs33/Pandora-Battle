#include "Component/Player/WeaponPresentationLoader.h"

#include "Definition/Item/ItemDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Weapon/WeaponBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogWeaponPresentation, Log, All);

namespace
{
	TArray<FSoftObjectPath> GetPresentationAssetPaths(const UItemDefinition& ItemDefinition)
	{
		TArray<FSoftObjectPath> AssetPaths;
		AssetPaths.Add(FSoftObjectPath(&ItemDefinition));
		ItemDefinition.GetWeaponPresentationAssetPaths(AssetPaths);
		return AssetPaths;
	}
}

bool FWeaponPresentationLoader::IsLoaded(const UItemDefinition* ItemDefinition, const bool bRequiresEquipMontage)
{
	if (!IsValid(ItemDefinition))
	{
		return false;
	}

	const FWeaponDefinitionData& WeaponData = ItemDefinition->WeaponData;
	return (WeaponData.Equip.ActorClass.IsNull() || WeaponData.Equip.ActorClass.IsValid())
		&& (!bRequiresEquipMontage
			|| WeaponData.Equip.EquipMontage.IsNull()
			|| WeaponData.Equip.EquipMontage.IsValid())
		&& (WeaponData.Equip.UnequipMontage.IsNull()
			|| WeaponData.Equip.UnequipMontage.IsValid())
		&& (WeaponData.Equip.AnimLayer.IsNull() || WeaponData.Equip.AnimLayer.IsValid())
		&& (WeaponData.Attack.AttackMontage.IsNull() || WeaponData.Attack.AttackMontage.IsValid())
		&& (WeaponData.HitReact.HitReactMontage.IsNull() || WeaponData.HitReact.HitReactMontage.IsValid())
		&& (WeaponData.Bow.WeaponMontage.IsNull() || WeaponData.Bow.WeaponMontage.IsValid())
		&& (WeaponData.Gun.ImpactDecalMaterial.IsNull() || WeaponData.Gun.ImpactDecalMaterial.IsValid());
}

bool FWeaponPresentationLoader::Request(
	UObject& Owner,
	const UItemDefinition* ItemDefinition,
	const bool bRequiresEquipMontage,
	FSimpleDelegate OnLoaded)
{
	if (!IsValid(ItemDefinition))
	{
		return false;
	}

	const FPrimaryAssetId ItemDefinitionId = ItemDefinition->GetPrimaryAssetId();
	if (!ItemDefinitionId.IsValid())
	{
		UE_LOG(LogWeaponPresentation, Error,
			TEXT("Cannot preload weapon presentation for '%s': invalid PrimaryAssetId."), *GetNameSafe(ItemDefinition));
		return false;
	}

	FStreamableManager& StreamableManager = UAssetManager::Get().GetStreamableManager();
	if (IsLoaded(ItemDefinition, bRequiresEquipMontage))
	{
		// 이미 메모리에 있어도 장착하는 동안 내려가지 않게 핸들을 잡아 둔다.
		if (!LoadHandles.Contains(ItemDefinitionId))
		{
			if (TSharedPtr<FStreamableHandle> RetainHandle =
				StreamableManager.RequestAsyncLoad(GetPresentationAssetPaths(*ItemDefinition)))
			{
				LoadHandles.Add(ItemDefinitionId, MoveTemp(RetainHandle));
			}
		}

		OnLoaded.ExecuteIfBound();
		return true;
	}

	if (OnLoaded.IsBound())
	{
		PendingCallbacks.FindOrAdd(ItemDefinitionId).Add(MoveTemp(OnLoaded));
	}

	if (const TSharedPtr<FStreamableHandle>* ExistingHandle = LoadHandles.Find(ItemDefinitionId))
	{
		if (ExistingHandle->IsValid() && !(*ExistingHandle)->HasLoadCompleted())
		{
			return true;
		}

		PendingCallbacks.Remove(ItemDefinitionId);
		UE_LOG(LogWeaponPresentation, Error,
			TEXT("Weapon presentation bundle completed but required assets are unavailable for '%s'."),
			*ItemDefinitionId.ToString());
		return false;
	}

	TSharedPtr<FStreamableHandle> LoadHandle = StreamableManager.RequestAsyncLoad(
		GetPresentationAssetPaths(*ItemDefinition),
		FStreamableDelegate::CreateWeakLambda(&Owner, [this, ItemDefinitionId, bRequiresEquipMontage]()
		{
			HandleLoaded(ItemDefinitionId, bRequiresEquipMontage);
		}));
	if (!LoadHandle.IsValid())
	{
		PendingCallbacks.Remove(ItemDefinitionId);
		UE_LOG(LogWeaponPresentation, Error,
			TEXT("Failed to start weapon presentation preload for '%s'."), *ItemDefinitionId.ToString());
		return false;
	}

	LoadHandles.Add(ItemDefinitionId, MoveTemp(LoadHandle));
	return true;
}

void FWeaponPresentationLoader::Reset()
{
	PendingCallbacks.Reset();
	for (TPair<FPrimaryAssetId, TSharedPtr<FStreamableHandle>>& HandlePair : LoadHandles)
	{
		if (HandlePair.Value.IsValid())
		{
			HandlePair.Value->ReleaseHandle();
		}
	}
	LoadHandles.Reset();
}

void FWeaponPresentationLoader::HandleLoaded(const FPrimaryAssetId ItemDefinitionId, const bool bRequiresEquipMontage)
{
	TArray<FSimpleDelegate> Callbacks;
	PendingCallbacks.RemoveAndCopyValue(ItemDefinitionId, Callbacks);

	const UItemDefinition* ItemDefinition = UAssetManager::Get().GetPrimaryAssetObject<UItemDefinition>(ItemDefinitionId);
	if (!IsLoaded(ItemDefinition, bRequiresEquipMontage))
	{
		UE_LOG(LogWeaponPresentation, Error,
			TEXT("Weapon presentation preload did not resolve all required assets for '%s'."), *ItemDefinitionId.ToString());
		return;
	}

	for (FSimpleDelegate& Callback : Callbacks)
	{
		Callback.ExecuteIfBound();
	}
}
