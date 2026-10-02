#include "Component/Player/EquipmentComponent.h"

#include "Common/LabGameplayTags.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Definition/Item/ItemDefinition.h"
#include "Definition/Player/CharacterActionDefinition.h"
#include "Definition/Settings/GameSettingDefinition.h"
#include "Engine/AssetManager.h"
#include "GameplayEffect.h"
#include "Item/ItemInstance.h"
#include "Pandora/PandoraLoadoutTypes.h"
#include "Settings/GameSettingsSubsystem.h"
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
	LatestRequestedWeapon = WeaponInstance;
	LatestRequestedWeaponDirection = Direction;
	bHasLatestWeaponRequest = true;
	const UPdAbilitySystemComponent* AbilitySystem = GetReadyAbilitySystem();
	if (AbilitySystem && AbilitySystem->HasMatchingGameplayTag(LabGameplayTags::Cooldown_EquipWeapon))
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
	LatestRequestedWeapon = nullptr;
	bHasLatestWeaponRequest = true;
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
	if (!RequestedWeaponId.IsValid())
	{
		return false;
	}

	if (IsDeathTransitionActive())
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
	if (!RequestedWeaponId.IsValid())
	{
		return false;
	}

	if (IsDeathTransitionActive())
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

	if (!ReplaceWeapon(WeaponDefinition, FGuid::NewGuid(), EEnum_Direction::Center))
	{
		return false;
	}
	RefreshCurrentWeaponAnimationLayer();
	return true;
}

// 무기 교체 직후 다음 교체까지의 간격을 쿨다운 효과로 건다. 쿨다운이 끝나면 막혔던 마지막 요청을 다시 시도한다.
bool UEquipmentComponent::ApplyEquipAbilityCooldown()
{
	if (!HasEquipmentAuthority())
	{
		return false;
	}

	UPdAbilitySystemComponent* AbilitySystem = GetReadyAbilitySystem();
	if (!AbilitySystem)
	{
		return false;
	}

	if (AbilitySystem->HasMatchingGameplayTag(LabGameplayTags::Cooldown_EquipWeapon))
	{
		return true;
	}

	TSoftObjectPtr<UCharacterActionDefinition> ActionDefinition(
		UCharacterActionDefinition::GetDefaultDefinitionPath());
	const UCharacterActionDefinition* LoadedDefinition =
		ActionDefinition.LoadSynchronous();
	if (!LoadedDefinition)
	{
		return false;
	}

	const float CooldownDuration = static_cast<float>(FMath::Max(
		LoadedDefinition->GetCooldownDuration(
			ECharacterActionType::PandoraWeaponSwap),
		0.0));
	if (CooldownDuration <= 0.0f)
	{
		return true;
	}

	FGameplayEffectContextHandle EffectContext = AbilitySystem->MakeEffectContext();
	EffectContext.AddSourceObject(const_cast<UCharacterActionDefinition*>(
		LoadedDefinition));
	const UGameSettingDefinition* SettingDefinition =
		UGameSettingsSubsystem::ResolveGameSettingDefinition(this);
	const TSubclassOf<UGameplayEffect> CooldownEffectClass =
		SettingDefinition
			? SettingDefinition->AbilityCooldownGameplayEffectClass
			: nullptr;
	if (!CooldownEffectClass)
	{
		return false;
	}

	FGameplayEffectSpecHandle CooldownSpec = AbilitySystem->MakeOutgoingSpec(
		CooldownEffectClass,
		1.0f,
		EffectContext);
	FGameplayTagContainer CooldownTags;
	CooldownTags.AddTag(LabGameplayTags::Cooldown_EquipWeapon);
	if (!CooldownSpec.IsValid() || !CooldownSpec.Data.IsValid())
	{
		return false;
	}

	CooldownSpec.Data->SetSetByCallerMagnitude(
		LabGameplayTags::Data_Cooldown,
		CooldownDuration);
	CooldownSpec.Data->DynamicGrantedTags.AppendTags(CooldownTags);
	CooldownSpec.Data->AppendDynamicAssetTags(CooldownTags);

	CooldownSpec.Data->AppendDynamicAssetTags(
		FGameplayTagContainer(LabGameplayTags::Effect_Policy_RemoveOnDeath));
	return AbilitySystem->ApplyGameplayEffectSpecToSelf(
		*CooldownSpec.Data.Get()).WasSuccessfullyApplied();
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
