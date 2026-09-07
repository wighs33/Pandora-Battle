#include "Component/Player/EquipmentComponent.h"

#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Common/LabGameplayTags.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Definition/Item/ItemDefinition.h"
#include "Engine/AssetManager.h"
#include "Item/ItemInstance.h"
#include "Pandora/PandoraLoadoutTypes.h"
#include "Weapon/WeaponBase.h"

// 서버가 확정한 슬롯의 무기를 준비하고 기존 무기의 해제·새 무기의 장착 능력을 이어 준다.
bool UEquipmentComponent::RequestWeaponSelectionForDirection(
	const EEnum_Direction Direction,
	UItemInstance* WeaponInstance)
{
	if (bEndingPlay || !HasEquipmentAuthority())
	{
		return false;
	}
	RefreshCachedReferences();
	if (CachedASC && CachedASC->HasMatchingGameplayTag(LabGameplayTags::Cooldown_EquipWeapon))
	{
		return false;
	}

	RequestedWeaponLoadoutDirection = PandoraLoadout::IsLoadoutDirection(Direction) ? Direction : EEnum_Direction::Center;

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
		const EEnum_Direction SelectedLoadoutDirection = RequestedWeaponLoadoutDirection;
		ClearRequestedWeaponInstance();
		const bool bHasRequestedLoadoutDirection = PandoraLoadout::IsLoadoutDirection(SelectedLoadoutDirection);
		const bool bDirectionChanged = bHasRequestedLoadoutDirection
			&& CurrentWeaponLoadoutDirection != SelectedLoadoutDirection;

		if (bDirectionChanged)
		{
			ApplyCurrentWeaponLoadoutDirection(SelectedWeaponId, SelectedLoadoutDirection);
		}
		return true;
	}

	RequestedWeaponId = SelectedWeaponId;

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
	if (bEndingPlay || !HasEquipmentAuthority())
	{
		return false;
	}
	RefreshCachedReferences();
	ClearRequestedWeaponInstance();

	// 아직 무기가 없어도 이전 로딩·선택은 취소한다. 빈 슬롯의 반복 해제에는 능력을 실행하지 않는다.
	if (!CurrentWeaponActor && !CurrentWeaponId.IsValid() && !CurrentWeaponDefinition)
	{
		return true;
	}

	const FGameplayTag UnequipAbilityTag = GetUnequipAbilityTag();

	FGameplayTagContainer UnequipTagContainer;
	UnequipTagContainer.AddTag(UnequipAbilityTag);
	if (HasActiveAbilityWithTags(UnequipTagContainer))
	{
		return false;
	}
	return TryActivateSingleAbilityTag(UnequipAbilityTag);
}

// 승인된 장착 능력이나 서버 AnimNotify가 대기 무기를 실제로 장착한다.
bool UEquipmentComponent::EquipWeapon()
{
	if (bEndingPlay || !HasEquipmentAuthority() || !RequestedWeaponId.IsValid())
	{
		return false;
	}
	const FGuid WeaponId = RequestedWeaponId;
	const EEnum_Direction Direction = RequestedWeaponLoadoutDirection;
	ClearRequestedWeaponInstance();
	UItemInstance* WeaponInstance = FindOwnedItemInstanceById(WeaponId);
	return WeaponInstance && EquipWeaponInternal(WeaponInstance, Direction);
}

// 장착 능력이 승인된 뒤 연출이 끊겨도 남아 있는 무기 전환을 마무리한다.
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
		ClearRequestedWeaponInstance();
		return false;
	}

	if (bEndingPlay || !HasEquipmentAuthority())
	{
		// 장착은 서버가 확정하므로 클라이언트에는 대기 요청을 남기지 않는다.
		ClearRequestedWeaponInstance();
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
		ClearRequestedWeaponInstance();
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

// 인벤토리 인스턴스를 사용하지 않는 AI의 기본 무기를 서버에서 직접 적용한다.
bool UEquipmentComponent::EquipWeaponDefinition(const UItemDefinition* WeaponDefinition)
{
	RefreshCachedReferences();

	if (bEndingPlay || !HasEquipmentAuthority())
	{
		return false;
	}

	if (!IsWeaponDefinitionEquipable(WeaponDefinition))
	{
		return false;
	}

	ClearRequestedWeaponInstance();
	if (!IsWeaponPresentationLoaded(WeaponDefinition))
	{
		const FPrimaryAssetId WeaponDefinitionId = WeaponDefinition->GetPrimaryAssetId();
		const uint32 RequestGeneration = WeaponPresentationRequestGeneration;
		return RequestWeaponPresentationLoad(
			WeaponDefinition,
			FSimpleDelegate::CreateWeakLambda(this, [this, WeaponDefinitionId, RequestGeneration]()
			{
				if (!HasEquipmentAuthority()
					|| bEndingPlay || WeaponPresentationRequestGeneration != RequestGeneration)
				{
					return;
				}

				if (const UItemDefinition* LoadedDefinition =
					UAssetManager::Get().GetPrimaryAssetObject<UItemDefinition>(WeaponDefinitionId))
				{
					EquipWeaponDefinition(LoadedDefinition);
				}
			}));
	}

	RequestWeaponPresentationLoad(WeaponDefinition, FSimpleDelegate());

	if (CurrentWeaponActor && CurrentWeaponDefinition == WeaponDefinition)
	{
		return true;
	}

	FEquippedItemStatSnapshot PendingStatSnapshot;
	if (!BuildItemDefinitionStatSnapshot(WeaponDefinition, PendingStatSnapshot)
		|| !ReplaceWeapon(WeaponDefinition, FGuid::NewGuid(), EEnum_Direction::Center, PendingStatSnapshot))
	{
		return false;
	}
	RefreshCurrentWeaponAnimationLayer();
	return true;
}

bool UEquipmentComponent::UnequipCurrentWeapon()
{
	if (bEndingPlay || !HasEquipmentAuthority())
	{
		return false;
	}
	return UnequipCurrentWeaponInternal();
}

bool UEquipmentComponent::ApplyCurrentWeaponLoadoutDirection(
	const FGuid WeaponId,
	const EEnum_Direction Direction)
{
	RefreshCachedReferences();

	const EEnum_Direction SanitizedDirection = PandoraLoadout::IsLoadoutDirection(Direction) ? Direction : EEnum_Direction::Center;
	if (bEndingPlay || !HasEquipmentAuthority())
	{
		return false;
	}

	if (!WeaponId.IsValid() || !IsCurrentWeapon(WeaponId) || !PandoraLoadout::IsLoadoutDirection(SanitizedDirection))
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
