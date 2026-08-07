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

namespace
{
	bool IsWeaponLoadoutDirection(const EEnum_Direction Direction)
	{
		return Direction == EEnum_Direction::Left
			|| Direction == EEnum_Direction::Up
			|| Direction == EEnum_Direction::Right;
	}

	EEnum_Direction SanitizeWeaponLoadoutDirection(const EEnum_Direction Direction)
	{
		return IsWeaponLoadoutDirection(Direction) ? Direction : EEnum_Direction::Center;
	}
}

bool UEquipmentComponent::SetRequestedWeaponInstance(UItemInstance* WeaponInstance)
{
	// =================================================================================================================

	RefreshCachedReferences();

	FGuid WeaponId;
	if (!ResolveWeaponIdFromInstance(WeaponInstance, WeaponId))
	{

		ClearRequestedWeapon();
		return false;
	}

	// =================================================================================================================

	RequestedWeaponId = WeaponId;

	if (!HasEquipmentAuthority())
	{

		ServerSetRequestedWeapon(WeaponId, RequestedWeaponLoadoutDirection);
	}

	return true;
}

void UEquipmentComponent::ClearRequestedWeaponInstance()
{
	ClearRequestedWeapon();
}

bool UEquipmentComponent::RequestWeaponSelectionForDirection(
	const EEnum_Direction Direction,
	UItemInstance* WeaponInstance)
{
	RefreshCachedReferences();
	if (CachedASC && CachedASC->HasMatchingGameplayTag(LabGameplayTags::Cooldown_EquipWeapon))
	{
		return false;
	}

	RequestedWeaponLoadoutDirection = SanitizeWeaponLoadoutDirection(Direction);

	const FGameplayTag EquipAbilityTag = GetEquipAbilityTag();
	const FGameplayTag UnequipAbilityTag = GetUnequipAbilityTag();

FGuid SelectedWeaponId;
	if (!ResolveWeaponIdFromInstance(WeaponInstance, SelectedWeaponId))
	{

		return false;
	}

	const UItemDefinition* SelectedWeaponDefinition =
		IsValid(WeaponInstance) ? WeaponInstance->ItemDefinition.Get() : nullptr;
	if (!IsWeaponPresentationLoaded(SelectedWeaponDefinition))
	{
		const uint32 RequestGeneration = ++WeaponPresentationRequestGeneration;
		const EEnum_Direction DeferredDirection = RequestedWeaponLoadoutDirection;
		return RequestWeaponPresentationLoad(
			SelectedWeaponDefinition,
			FSimpleDelegate::CreateWeakLambda(
				this,
				[this, SelectedWeaponId, DeferredDirection, RequestGeneration]()
				{
					if (WeaponPresentationRequestGeneration != RequestGeneration)
					{
						return;
					}

					RefreshCachedReferences();
					if (UItemInstance* LoadedWeaponInstance =
						FindOwnedItemInstanceById(SelectedWeaponId))
					{
						RequestWeaponSelectionForDirection(
							DeferredDirection,
							LoadedWeaponInstance);
					}
				}));
	}

	++WeaponPresentationRequestGeneration;
	RequestWeaponPresentationLoad(SelectedWeaponDefinition, FSimpleDelegate());

	if (IsCurrentWeapon(SelectedWeaponId))
	{
		const bool bHasRequestedLoadoutDirection = IsWeaponLoadoutDirection(RequestedWeaponLoadoutDirection);
		const bool bDirectionChanged = bHasRequestedLoadoutDirection
			&& CurrentWeaponLoadoutDirection != RequestedWeaponLoadoutDirection;

		if (bHasRequestedLoadoutDirection && !HasEquipmentAuthority())
		{
			ServerSetCurrentWeaponLoadoutDirection(SelectedWeaponId, RequestedWeaponLoadoutDirection);
			return true;
		}

		if (bDirectionChanged)
		{
			ApplyCurrentWeaponLoadoutDirection(SelectedWeaponId, RequestedWeaponLoadoutDirection);
		}

		return true;
	}

	if (!SetRequestedWeaponInstance(WeaponInstance))
	{

		return false;
	}

	if (CurrentWeaponActor)
	{
		FGameplayTagContainer UnequipTagContainer;
		UnequipTagContainer.AddTag(UnequipAbilityTag);
		if (HasActiveAbilityWithTags(UnequipTagContainer))
		{

			return false;
		}

		return TryActivateSingleAbilityTag(UnequipAbilityTag);
	}

	return TryActivateSingleAbilityTag(EquipAbilityTag);
}

bool UEquipmentComponent::RequestWeaponUnequip()
{
	RefreshCachedReferences();
	++WeaponPresentationRequestGeneration;
	PendingDefinitionEquipAssetId = FPrimaryAssetId();
	ClearRequestedWeaponInstance();

	const FGameplayTag UnequipAbilityTag = GetUnequipAbilityTag();

FGameplayTagContainer UnequipTagContainer;
	UnequipTagContainer.AddTag(UnequipAbilityTag);
	if (HasActiveAbilityWithTags(UnequipTagContainer))
	{

		return false;
	}

	return TryActivateSingleAbilityTag(UnequipAbilityTag);
}

bool UEquipmentComponent::EquipWeapon()
{
	if (!RequestedWeaponId.IsValid())
	{

		return false;
	}

	if (!GetOwner())
	{
		return false;
	}

	if (!HasEquipmentAuthority())
	{

		ServerEquipWeapon();
		ClearRequestedWeapon();
		return true;
	}

	const FGuid WeaponId = RequestedWeaponId;
	const EEnum_Direction WeaponLoadoutDirection = RequestedWeaponLoadoutDirection;
	ClearRequestedWeapon();

	UItemInstance* WeaponInstance = FindOwnedItemInstanceById(WeaponId);
	if (!WeaponInstance)
	{

		return false;
	}

	return EquipWeaponInternal(WeaponInstance, WeaponLoadoutDirection);
}

bool UEquipmentComponent::CompletePendingWeaponSelectionWithoutAnimation()
{
	RefreshCachedReferences();

	if (!RequestedWeaponId.IsValid())
	{
		return false;
	}

	const bool bDeathTransitionActive = CachedASC
		&& (CachedASC->HasMatchingGameplayTag(LabGameplayTags::State_Dead)
			|| CachedASC->GetNumericAttribute(UBasicAttributeSet::GetHealthAttribute()) <= 0.0f);
	if (bDeathTransitionActive)
	{
		ClearRequestedWeapon();
		return false;
	}

	if (!HasEquipmentAuthority())
	{
		// The authoritative copy completes the same pending request and
		// replicates CurrentWeapon state back. Do not leave a stale local
		// request after its predicted equipment ability was cancelled.
		ClearRequestedWeapon();
		return true;
	}

	if (!EquipWeapon())
	{
		return false;
	}

	ApplyEquipAbilityCooldown();
	RefreshCurrentWeaponAnimationLayer();
	return true;
}

bool UEquipmentComponent::TryResumePendingWeaponSelection()
{
	RefreshCachedReferences();

	if (!RequestedWeaponId.IsValid())
	{
		return false;
	}

	const bool bDeathTransitionActive = CachedASC
		&& (CachedASC->HasMatchingGameplayTag(LabGameplayTags::State_Dead)
			|| CachedASC->GetNumericAttribute(UBasicAttributeSet::GetHealthAttribute()) <= 0.0f);
	if (bDeathTransitionActive)
	{
		ClearRequestedWeapon();
		return false;
	}

	const FGameplayTag EquipAbilityTag = GetEquipAbilityTag();
	const FGameplayTag UnequipAbilityTag = GetUnequipAbilityTag();
	FGameplayTagContainer EquipmentTransitionTags;
	EquipmentTransitionTags.AddTag(EquipAbilityTag);
	EquipmentTransitionTags.AddTag(UnequipAbilityTag);
	if (HasActiveAbilityWithTags(EquipmentTransitionTags))
	{
		return true;
	}

	return CurrentWeaponActor
		? TryActivateSingleAbilityTag(UnequipAbilityTag)
		: TryActivateSingleAbilityTag(EquipAbilityTag);
}

bool UEquipmentComponent::EquipWeaponDefinition(const UItemDefinition* WeaponDefinition)
{
	RefreshCachedReferences();

	if (!HasEquipmentAuthority())
	{

		return false;
	}

	if (!IsWeaponDefinitionEquipable(WeaponDefinition))
	{

		return false;
	}

	if (!IsWeaponPresentationLoaded(WeaponDefinition))
	{
		const FPrimaryAssetId WeaponDefinitionId = WeaponDefinition->GetPrimaryAssetId();
		PendingDefinitionEquipAssetId = WeaponDefinitionId;
		return RequestWeaponPresentationLoad(
			WeaponDefinition,
			FSimpleDelegate::CreateWeakLambda(this, [this, WeaponDefinitionId]()
			{
				if (!HasEquipmentAuthority()
					|| PendingDefinitionEquipAssetId != WeaponDefinitionId)
				{
					return;
				}

				PendingDefinitionEquipAssetId = FPrimaryAssetId();
				if (const UItemDefinition* LoadedDefinition =
					UAssetManager::Get().GetPrimaryAssetObject<UItemDefinition>(WeaponDefinitionId))
				{
					EquipWeaponDefinition(LoadedDefinition);
				}
			}));
	}

	PendingDefinitionEquipAssetId = FPrimaryAssetId();
	RequestWeaponPresentationLoad(WeaponDefinition, FSimpleDelegate());

	if (CurrentWeaponActor && CurrentWeaponDefinition == WeaponDefinition)
	{

		return true;
	}

	TSubclassOf<AWeaponBase> WeaponClass = LoadWeaponActorClass(WeaponDefinition);
	if (!WeaponClass)
	{

		return false;
	}

	FEquippedItemStatSnapshot PendingStatSnapshot;
	if (!BuildItemDefinitionStatSnapshot(WeaponDefinition, PendingStatSnapshot))
	{

		return false;
	}

	const bool bHadCurrentWeaponState = CurrentWeaponId.IsValid()
		|| CurrentWeaponActor
		|| CurrentWeaponDefinition
		|| CurrentWeaponStatSnapshot.HasAnyMagnitude()
		|| CurrentWeaponTagEffectHandle.IsValid();
	if (!UnequipCurrentWeaponInternal() && bHadCurrentWeaponState)
	{
		return false;
	}

	AWeaponBase* SpawnedWeapon = SpawnAndAttachWeaponActor(WeaponClass, WeaponDefinition);
	if (!SpawnedWeapon)
	{

		return false;
	}

	ApplyAndStoreWeaponStats(WeaponDefinition, PendingStatSnapshot);
	ApplyCurrentWeaponTagEffect(WeaponDefinition);
	CommitCurrentWeaponState(FGuid::NewGuid(), SpawnedWeapon, WeaponDefinition, EEnum_Direction::Center);
	if (ACharacterBase* CharacterOwner = CachedOwner.Get())
	{
		if (TSubclassOf<UAnimInstance> EquipAnimLayer = GetCachedEquipAnimLayer(WeaponDefinition))
		{
			CharacterOwner->SetCurrentAnimLayer(EquipAnimLayer);
		}
	}

	return true;
}

bool UEquipmentComponent::UnequipCurrentWeapon()
{
	if (!HasEquipmentAuthority())
	{
		return false;
	}

	return UnequipCurrentWeaponInternal();
}

void UEquipmentComponent::ServerEquipWeapon_Implementation()
{
	RefreshCachedReferences();
	EquipWeapon();
}

void UEquipmentComponent::ServerSetRequestedWeapon_Implementation(
	const FGuid WeaponId,
	const EEnum_Direction RequestedDirection)
{
	// =================================================================================================================

	RefreshCachedReferences();

	UItemInstance* FoundItem = nullptr;
	const UItemDefinition* ItemDefinition = nullptr;
	if (!ResolveOwnedWeaponById(WeaponId, FoundItem, ItemDefinition))
	{

		ClearRequestedWeapon();
		return;
	}

	// =================================================================================================================

	++WeaponPresentationRequestGeneration;
	RequestedWeaponId = WeaponId;
	RequestedWeaponLoadoutDirection = SanitizeWeaponLoadoutDirection(RequestedDirection);
}

bool UEquipmentComponent::RequestCurrentWeaponLoadoutDirection(
	const EEnum_Direction Direction,
	UItemInstance* WeaponInstance)
{
	RefreshCachedReferences();

	const EEnum_Direction SanitizedDirection = SanitizeWeaponLoadoutDirection(Direction);
	if (!IsWeaponLoadoutDirection(SanitizedDirection))
	{

		return false;
	}

	FGuid WeaponId;
	if (!ResolveWeaponIdFromInstance(WeaponInstance, WeaponId) || !IsCurrentWeapon(WeaponId))
	{

		return false;
	}

	if (!HasEquipmentAuthority())
	{
		ServerSetCurrentWeaponLoadoutDirection(WeaponId, SanitizedDirection);
		return true;
	}

	return ApplyCurrentWeaponLoadoutDirection(WeaponId, SanitizedDirection);
}

void UEquipmentComponent::ServerSetCurrentWeaponLoadoutDirection_Implementation(
	const FGuid WeaponId,
	const EEnum_Direction RequestedDirection)
{
	RefreshCachedReferences();
	ApplyCurrentWeaponLoadoutDirection(WeaponId, RequestedDirection);
}

bool UEquipmentComponent::ApplyCurrentWeaponLoadoutDirection(
	const FGuid WeaponId,
	const EEnum_Direction Direction)
{
	RefreshCachedReferences();

	const EEnum_Direction SanitizedDirection = SanitizeWeaponLoadoutDirection(Direction);
	if (!HasEquipmentAuthority())
	{

		return false;
	}

	if (!WeaponId.IsValid() || !IsCurrentWeapon(WeaponId) || !IsWeaponLoadoutDirection(SanitizedDirection))
	{

		return false;
	}

	if (CurrentWeaponLoadoutDirection == SanitizedDirection)
	{

		return true;
	}

	CurrentWeaponLoadoutDirection = SanitizedDirection;
	MarkCurrentWeaponStateDirty(false, false, false, true);
	RefreshPandoraForWeaponChange();
	NotifyCurrentWeaponStateChanged();

	return true;
}
