#include "Component/Player/EquipmentComponent.h"

#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Component/Character/CharacterAbilityRuntimeComponent.h"
#include "Character/CharacterBase.h"
#include "Common/Enum_Operation.h"
#include "Common/LabGameplayTags.h"
#include "Definition/Common/ProjectTagConfig.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameplayEffect.h"
#include "Component/Item/InventoryComponent.h"
#include "Definition/Item/ItemDefinition.h"
#include "Item/ItemInstance.h"
#include "Mode/PdPlayerState.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Component/Pandora/PandoraComponent.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "Definition/Settings/GameSettingDefinition.h"
#include "Settings/GameSettingsSubsystem.h"
#include "Weapon/WeaponBase.h"

UAnimMontage* UEquipmentComponent::GetCachedEquipMontage(const UItemDefinition* ItemDefinition) const
{
	if (!ItemDefinition)
	{
		return nullptr;
	}

	if (CachedEquipDataItemDefinition != ItemDefinition)
	{
		CachedEquipDataItemDefinition = ItemDefinition;
		CachedEquipMontage.Reset();
		CachedEquipAnimLayerClass.Reset();
	}

	if (!CachedEquipMontage.IsValid() && !ItemDefinition->WeaponData.Equip.EquipMontage.IsNull())
	{
		CachedEquipMontage = ItemDefinition->WeaponData.Equip.EquipMontage.Get();
	}

	return CachedEquipMontage.Get();
}

TSubclassOf<UAnimInstance> UEquipmentComponent::GetCachedEquipAnimLayer(const UItemDefinition* ItemDefinition) const
{
	if (!ItemDefinition)
	{
		return nullptr;
	}

	if (CachedEquipDataItemDefinition != ItemDefinition)
	{
		CachedEquipDataItemDefinition = ItemDefinition;
		CachedEquipMontage.Reset();
		CachedEquipAnimLayerClass.Reset();
	}

	if (!CachedEquipAnimLayerClass.IsValid() && !ItemDefinition->WeaponData.Equip.AnimLayer.IsNull())
	{
		CachedEquipAnimLayerClass = ItemDefinition->WeaponData.Equip.AnimLayer.Get();
	}

	return CachedEquipAnimLayerClass.Get();
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
	if (TSubclassOf<UAnimInstance> EquipAnimLayer = GetCachedEquipAnimLayer(WeaponDefinition))
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
	PendingDefinitionEquipAssetId = FPrimaryAssetId();
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

UAnimMontage* UEquipmentComponent::GetCachedUnequipMontage(const UItemDefinition* ItemDefinition) const
{
	if (!ItemDefinition)
	{
		return nullptr;
	}

	if (CachedUnequipDataItemDefinition != ItemDefinition)
	{
		CachedUnequipDataItemDefinition = ItemDefinition;
		CachedUnequipMontage.Reset();
	}

	if (!CachedUnequipMontage.IsValid() && !ItemDefinition->WeaponData.Equip.UnequipMontage.IsNull())
	{
		CachedUnequipMontage = ItemDefinition->WeaponData.Equip.UnequipMontage.Get();
	}

	return CachedUnequipMontage.Get();
}

UAnimMontage* UEquipmentComponent::GetCachedAttackMontage(const UItemDefinition* ItemDefinition) const
{
	if (!ItemDefinition)
	{
		return nullptr;
	}

	if (CachedAttackDataItemDefinition != ItemDefinition)
	{
		CachedAttackDataItemDefinition = ItemDefinition;
		CachedAttackMontage.Reset();
	}

	if (!CachedAttackMontage.IsValid() && !ItemDefinition->WeaponData.Attack.AttackMontage.IsNull())
	{
		CachedAttackMontage = ItemDefinition->WeaponData.Attack.AttackMontage.Get();
	}

	return CachedAttackMontage.Get();
}

UAnimMontage* UEquipmentComponent::GetCachedHitReactMontage(const UItemDefinition* ItemDefinition) const
{
	if (!ItemDefinition)
	{
		return nullptr;
	}

	if (CachedHitReactDataItemDefinition != ItemDefinition)
	{
		CachedHitReactDataItemDefinition = ItemDefinition;
		CachedHitReactMontage.Reset();
	}

	if (!CachedHitReactMontage.IsValid() && !ItemDefinition->WeaponData.HitReact.HitReactMontage.IsNull())
	{
		CachedHitReactMontage = ItemDefinition->WeaponData.HitReact.HitReactMontage.Get();
	}

	return CachedHitReactMontage.Get();
}

TSubclassOf<AWeaponBase> UEquipmentComponent::LoadWeaponActorClass(const UItemDefinition* ItemDefinition) const
{
	if (!ItemDefinition)
	{

		return nullptr;
	}

	if (CachedWeaponActorClassItemDefinition != ItemDefinition)
	{
		CachedWeaponActorClassItemDefinition = ItemDefinition;
		CachedWeaponActorClass.Reset();
	}

	if (!CachedWeaponActorClass.IsValid() && !ItemDefinition->WeaponData.Equip.ActorClass.IsNull())
	{
		CachedWeaponActorClass = ItemDefinition->WeaponData.Equip.ActorClass.Get();
	}

	UClass* WeaponClass = CachedWeaponActorClass.Get();
	if (!WeaponClass)
	{

		return nullptr;
	}

	return WeaponClass;
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

	// =================================================================================================================

	const FName AttachSocketName = ItemDefinition ? ItemDefinition->WeaponData.Equip.GetResolvedAttachSocketName() : NAME_None;
	WeaponActor->AttachToComponent(
		OwnerMesh,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		AttachSocketName);

}
