#include "Component/Player/WeaponPresentationLoader.h"

#include "Data/ContentDataSubsystem.h"
#include "Data/ContentLease.h"
#include "Definition/Item/ItemDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
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
		&& (!bRequiresEquipMontage || WeaponData.Equip.EquipMontage.IsNull() || WeaponData.Equip.EquipMontage.IsValid())
		&& (WeaponData.Equip.UnequipMontage.IsNull() || WeaponData.Equip.UnequipMontage.IsValid())
		&& (WeaponData.Equip.AnimLayer.IsNull() || WeaponData.Equip.AnimLayer.IsValid())
		&& (WeaponData.Attack.AttackMontage.IsNull() || WeaponData.Attack.AttackMontage.IsValid())
		&& (WeaponData.HitReact.HitReactMontage.IsNull() || WeaponData.HitReact.HitReactMontage.IsValid())
		&& (WeaponData.Bow.WeaponMontage.IsNull() || WeaponData.Bow.WeaponMontage.IsValid())
		&& (WeaponData.Gun.ImpactDecalMaterial.IsNull() || WeaponData.Gun.ImpactDecalMaterial.IsValid());
}

bool FWeaponPresentationLoader::Request(UObject& Owner, const UItemDefinition* ItemDefinition,
	const bool bRequiresEquipMontage, FSimpleDelegate OnLoaded)
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

	const UWorld* World = Owner.GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UContentDataSubsystem* ContentSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
	if (IsLoaded(ItemDefinition, bRequiresEquipMontage))
	{
		// 이미 메모리에 있어도 장착하는 동안 내려가지 않게 lease를 잡아 둔다.
		if (ContentSubsystem && !LoadLeases.Contains(ItemDefinitionId))
		{
			TSharedPtr<FContentLease> RetainLease =
				ContentSubsystem->AcquireContent(GetPresentationAssetPaths(*ItemDefinition));
			if (!RetainLease->HasFailed())
			{
				LoadLeases.Add(ItemDefinitionId, MoveTemp(RetainLease));
			}
		}

		OnLoaded.ExecuteIfBound();
		return true;
	}

	if (OnLoaded.IsBound())
	{
		PendingCallbacks.FindOrAdd(ItemDefinitionId).Add(MoveTemp(OnLoaded));
	}

	if (const TSharedPtr<FContentLease>* ExistingLease = LoadLeases.Find(ItemDefinitionId))
	{
		if (ExistingLease->IsValid() && (*ExistingLease)->IsLoading())
		{
			return true;
		}

		PendingCallbacks.Remove(ItemDefinitionId);
		UE_LOG(LogWeaponPresentation, Error,
			TEXT("Weapon presentation bundle completed but required assets are unavailable for '%s'."),
			*ItemDefinitionId.ToString());
		return false;
	}

	if (!ContentSubsystem)
	{
		PendingCallbacks.Remove(ItemDefinitionId);
		return false;
	}

	TSharedPtr<FContentLease> LoadLease = ContentSubsystem->AcquireContent(GetPresentationAssetPaths(*ItemDefinition),
		FSimpleDelegate::CreateWeakLambda(&Owner, [this, ItemDefinitionId, bRequiresEquipMontage]()
		{
			HandleLoaded(ItemDefinitionId, bRequiresEquipMontage);
		}));
	if (LoadLease->HasFailed())
	{
		PendingCallbacks.Remove(ItemDefinitionId);
		UE_LOG(LogWeaponPresentation, Error,
			TEXT("Failed to start weapon presentation preload for '%s'."), *ItemDefinitionId.ToString());
		return false;
	}

	LoadLeases.Add(ItemDefinitionId, MoveTemp(LoadLease));
	return true;
}

void FWeaponPresentationLoader::Reset()
{
	PendingCallbacks.Reset();
	LoadLeases.Reset();
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
