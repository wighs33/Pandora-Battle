#include "Component/Player/EquipmentComponent.h"

#include "Animation/AnimInstance.h"
#include "Character/CharacterBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Definition/Item/ItemDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "Weapon/WeaponBase.h"

// 로딩은 StreamableHandle이 맡고, 준비된 참조는 Soft Pointer에서 바로 읽는다.
UAnimMontage* UEquipmentComponent::GetLoadedEquipMontage(const UItemDefinition* ItemDefinition) const
{
	return ItemDefinition ? ItemDefinition->WeaponData.Equip.EquipMontage.Get() : nullptr;
}

TSubclassOf<UAnimInstance> UEquipmentComponent::GetLoadedEquipAnimLayer(const UItemDefinition* ItemDefinition) const
{
	return ItemDefinition ? ItemDefinition->WeaponData.Equip.AnimLayer.Get() : nullptr;
}

void UEquipmentComponent::RefreshCurrentWeaponAnimationLayer()
{
	RefreshCachedReferences();

	ACharacterBase* CharacterOwner = CachedOwner.Get();
	if (!CharacterOwner)
	{
		return;
	}

	const UItemDefinition* WeaponDefinition = GetCurrentWeaponDefinition();
	if (TSubclassOf<UAnimInstance> EquipAnimLayer = GetLoadedEquipAnimLayer(WeaponDefinition))
	{
		CharacterOwner->SetCurrentAnimLayer(EquipAnimLayer);
		return;
	}

	CharacterOwner->ResetAnimationToDefault();
}

bool UEquipmentComponent::IsWeaponPresentationLoaded(const UItemDefinition* ItemDefinition) const
{
	if (!IsValid(ItemDefinition))
	{
		return false;
	}

	const FWeaponDefinitionData& WeaponData = ItemDefinition->WeaponData;
	const bool bRequiresEquipMontage = !ShouldEquipWeaponsWithoutAnimation();
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

bool UEquipmentComponent::RequestWeaponPresentationLoad(
	const UItemDefinition* ItemDefinition,
	FSimpleDelegate OnLoaded)
{
	if (!IsValid(ItemDefinition))
	{
		return false;
	}

	const FPrimaryAssetId ItemDefinitionId = ItemDefinition->GetPrimaryAssetId();
	if (!ItemDefinitionId.IsValid())
	{
		UE_LOG(
			EquipmentComponentLog,
			Error,
			TEXT("Cannot preload weapon presentation for '%s': invalid PrimaryAssetId."),
			*GetNameSafe(ItemDefinition));
		return false;
	}

	if (IsWeaponPresentationLoaded(ItemDefinition))
	{
		if (!WeaponPresentationLoadHandles.Contains(ItemDefinitionId))
		{
			TArray<FSoftObjectPath> PresentationAssetPaths;
			PresentationAssetPaths.Add(FSoftObjectPath(ItemDefinition));
			ItemDefinition->GetWeaponPresentationAssetPaths(PresentationAssetPaths);
			if (TSharedPtr<FStreamableHandle> RetainHandle =
				UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
					PresentationAssetPaths))
			{
				WeaponPresentationLoadHandles.Add(ItemDefinitionId, MoveTemp(RetainHandle));
			}
		}

		OnLoaded.ExecuteIfBound();
		return true;
	}

	if (OnLoaded.IsBound())
	{
		PendingWeaponPresentationCallbacks.FindOrAdd(ItemDefinitionId).Add(MoveTemp(OnLoaded));
	}

	if (const TSharedPtr<FStreamableHandle>* ExistingHandle =
		WeaponPresentationLoadHandles.Find(ItemDefinitionId))
	{
		if (ExistingHandle->IsValid() && !(*ExistingHandle)->HasLoadCompleted())
		{
			return true;
		}

		PendingWeaponPresentationCallbacks.Remove(ItemDefinitionId);
		UE_LOG(
			EquipmentComponentLog,
			Error,
			TEXT("Weapon presentation bundle completed but required assets are unavailable for '%s'."),
			*ItemDefinitionId.ToString());
		return false;
	}

	TArray<FSoftObjectPath> PresentationAssetPaths;
	PresentationAssetPaths.Add(FSoftObjectPath(ItemDefinition));
	ItemDefinition->GetWeaponPresentationAssetPaths(PresentationAssetPaths);
	TSharedPtr<FStreamableHandle> LoadHandle =
		UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
		PresentationAssetPaths,
		FStreamableDelegate::CreateUObject(
			this,
			&ThisClass::HandleWeaponPresentationLoaded,
			ItemDefinitionId));
	if (!LoadHandle.IsValid())
	{
		PendingWeaponPresentationCallbacks.Remove(ItemDefinitionId);
		UE_LOG(
			EquipmentComponentLog,
			Error,
			TEXT("Failed to start weapon presentation preload for '%s'."),
			*ItemDefinitionId.ToString());
		return false;
	}

	WeaponPresentationLoadHandles.Add(ItemDefinitionId, MoveTemp(LoadHandle));
	return true;
}

void UEquipmentComponent::HandleWeaponPresentationLoaded(const FPrimaryAssetId ItemDefinitionId)
{
	const UItemDefinition* ItemDefinition =
		UAssetManager::Get().GetPrimaryAssetObject<UItemDefinition>(ItemDefinitionId);
	TArray<FSimpleDelegate> Callbacks;
	if (TArray<FSimpleDelegate>* PendingCallbacks =
		PendingWeaponPresentationCallbacks.Find(ItemDefinitionId))
	{
		Callbacks = MoveTemp(*PendingCallbacks);
		PendingWeaponPresentationCallbacks.Remove(ItemDefinitionId);
	}

	if (!IsWeaponPresentationLoaded(ItemDefinition))
	{
		UE_LOG(
			EquipmentComponentLog,
			Error,
			TEXT("Weapon presentation preload did not resolve all required assets for '%s'."),
			*ItemDefinitionId.ToString());
		return;
	}

	for (FSimpleDelegate& Callback : Callbacks)
	{
		Callback.ExecuteIfBound();
	}
}

void UEquipmentComponent::RefreshCurrentWeaponPresentation()
{
	const UItemDefinition* ItemDefinition = GetCurrentWeaponDefinition();
	if (!IsValid(ItemDefinition))
	{
		RefreshCurrentWeaponAnimationLayer();
		return;
	}

	if (IsWeaponPresentationLoaded(ItemDefinition))
	{
		RequestWeaponPresentationLoad(ItemDefinition, FSimpleDelegate());
		RefreshCurrentWeaponAnimationLayer();
		return;
	}

	const FPrimaryAssetId ItemDefinitionId = ItemDefinition->GetPrimaryAssetId();
	const bool bLoadRequested = RequestWeaponPresentationLoad(
		ItemDefinition,
		FSimpleDelegate::CreateWeakLambda(this, [this, ItemDefinitionId]()
		{
			const UItemDefinition* CurrentDefinition = GetCurrentWeaponDefinition();
			if (IsValid(CurrentDefinition)
				&& CurrentDefinition->GetPrimaryAssetId() == ItemDefinitionId)
			{
				RefreshCurrentWeaponAnimationLayer();
			}
		}));
	if (!bLoadRequested)
	{
		RefreshCurrentWeaponAnimationLayer();
	}
}

void UEquipmentComponent::ReleaseWeaponPresentationLoads()
{
	++WeaponPresentationRequestGeneration;
	PendingWeaponPresentationCallbacks.Reset();
	for (TPair<FPrimaryAssetId, TSharedPtr<FStreamableHandle>>& HandlePair :
		WeaponPresentationLoadHandles)
	{
		if (HandlePair.Value.IsValid())
		{
			HandlePair.Value->ReleaseHandle();
		}
	}
	WeaponPresentationLoadHandles.Reset();
}

UAnimMontage* UEquipmentComponent::GetLoadedUnequipMontage(const UItemDefinition* ItemDefinition) const
{
	return ItemDefinition ? ItemDefinition->WeaponData.Equip.UnequipMontage.Get() : nullptr;
}

UAnimMontage* UEquipmentComponent::GetLoadedAttackMontage(const UItemDefinition* ItemDefinition) const
{
	return ItemDefinition ? ItemDefinition->WeaponData.Attack.AttackMontage.Get() : nullptr;
}

UAnimMontage* UEquipmentComponent::GetLoadedHitReactMontage(const UItemDefinition* ItemDefinition) const
{
	return ItemDefinition ? ItemDefinition->WeaponData.HitReact.HitReactMontage.Get() : nullptr;
}

TSubclassOf<AWeaponBase> UEquipmentComponent::GetLoadedWeaponActorClass(const UItemDefinition* ItemDefinition) const
{
	return ItemDefinition ? ItemDefinition->WeaponData.Equip.ActorClass.Get() : nullptr;
}

AWeaponBase* UEquipmentComponent::SpawnAndAttachWeaponActor(TSubclassOf<AWeaponBase> WeaponClass, const UItemDefinition* ItemDefinition) const
{
	ACharacterBase* CharacterOwner = CachedOwner.Get();
	USkeletalMeshComponent* OwnerMesh = CharacterOwner ? CharacterOwner->GetMesh() : nullptr;
	UWorld* World = GetWorld();
	if (!WeaponClass || !ItemDefinition || !CharacterOwner || !OwnerMesh || !World)
	{
		return nullptr;
	}

	FTransform SpawnTransform = OwnerMesh->GetComponentTransform();
	const FName AttachSocketName = ItemDefinition->WeaponData.Equip.GetResolvedAttachSocketName();
	if (AttachSocketName != NAME_None && OwnerMesh->DoesSocketExist(AttachSocketName))
	{
		SpawnTransform = OwnerMesh->GetSocketTransform(AttachSocketName);
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = CharacterOwner;
	SpawnParams.Instigator = CharacterOwner;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AWeaponBase* SpawnedWeapon = World->SpawnActor<AWeaponBase>(WeaponClass, SpawnTransform, SpawnParams);
	if (!SpawnedWeapon)
	{
		return nullptr;
	}

	SpawnedWeapon->InitializeFromItemDefinition(ItemDefinition);
	SpawnedWeapon->SetReplicates(true);
	AttachWeaponToOwner(SpawnedWeapon, ItemDefinition);
	SpawnedWeapon->ForceNetUpdate();
	return SpawnedWeapon;
}

void UEquipmentComponent::AttachWeaponToOwner(AWeaponBase* WeaponActor, const UItemDefinition* ItemDefinition) const
{
	ACharacterBase* CharacterOwner = CachedOwner.Get();
	USkeletalMeshComponent* OwnerMesh = CharacterOwner ? CharacterOwner->GetMesh() : nullptr;
	if (!WeaponActor || !OwnerMesh)
	{
		return;
	}

	const FName AttachSocketName = ItemDefinition ? ItemDefinition->WeaponData.Equip.GetResolvedAttachSocketName() : NAME_None;
	WeaponActor->AttachToComponent(
		OwnerMesh,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		AttachSocketName);
}
