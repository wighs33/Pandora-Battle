#include "Component/Player/EquipmentComponent.h"

#include "Animation/AnimInstance.h"
#include "Character/CharacterBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Definition/Item/ItemDefinition.h"
#include "Engine/World.h"
#include "Weapon/WeaponBase.h"

TSubclassOf<UAnimInstance> UEquipmentComponent::GetLoadedEquipAnimLayer(const UItemDefinition* ItemDefinition) const
{
	return ItemDefinition ? ItemDefinition->WeaponData.Equip.AnimLayer.Get() : nullptr;
}

void UEquipmentComponent::RefreshCurrentWeaponAnimationLayer()
{
	ACharacterBase* CharacterOwner = GetCharacter();
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
	return FWeaponPresentationLoader::IsLoaded(ItemDefinition, !ShouldEquipWeaponsWithoutAnimation());
}

bool UEquipmentComponent::RequestWeaponPresentationLoad(
	const UItemDefinition* ItemDefinition,
	FSimpleDelegate OnLoaded)
{
	return PresentationLoader.Request(*this, ItemDefinition, !ShouldEquipWeaponsWithoutAnimation(), MoveTemp(OnLoaded));
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

AWeaponBase* UEquipmentComponent::SpawnAndAttachWeaponActor(TSubclassOf<AWeaponBase> WeaponClass, const UItemDefinition* ItemDefinition) const
{
	ACharacterBase* CharacterOwner = GetCharacter();
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
	ACharacterBase* CharacterOwner = GetCharacter();
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
