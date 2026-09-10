#include "Component/Player/EquipmentComponent.h"

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
#include "Definition/Player/CharacterActionDefinition.h"
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
bool AreStatMagnitudeMapsEqual(
	const TMap<FGameplayTag, float>& Left,
	const TMap<FGameplayTag, float>& Right)
{
	if (Left.Num() != Right.Num())
	{
		return false;
	}

	for (const TPair<FGameplayTag, float>& Pair : Left)
	{
		const float* RightMagnitude = Right.Find(Pair.Key);
		if (!RightMagnitude || !FMath::IsNearlyEqual(Pair.Value, *RightMagnitude))
		{
			return false;
		}
	}
	return true;
}

bool AreStatSnapshotsEqual(
	const FEquippedItemStatSnapshot& Left,
	const FEquippedItemStatSnapshot& Right)
{
	return AreStatMagnitudeMapsEqual(
			Left.BaseStatMagnitudes,
			Right.BaseStatMagnitudes)
		&& AreStatMagnitudeMapsEqual(
			Left.EnhancedStatMagnitudes,
			Right.EnhancedStatMagnitudes)
		&& AreStatMagnitudeMapsEqual(
			Left.NonAttributeStatMagnitudes,
			Right.NonAttributeStatMagnitudes);
}

void AddEquipmentStatMagnitude(
	TMap<FGameplayTag, float>& StatMagnitudes,
	const FGameplayTag StatTag,
	const float Magnitude)
{
	if (!StatTag.IsValid()
		|| !FMath::IsFinite(Magnitude)
		|| FMath::IsNearlyZero(Magnitude))
	{
		return;
	}

	const double CombinedMagnitude =
		static_cast<double>(StatMagnitudes.FindRef(StatTag))
		+ static_cast<double>(Magnitude);
	const double MaxFloatMagnitude =
		static_cast<double>(TNumericLimits<float>::Max());
	const float SafeMagnitude = static_cast<float>(FMath::Clamp(
		CombinedMagnitude,
		-MaxFloatMagnitude,
		MaxFloatMagnitude));
	if (FMath::IsNearlyZero(SafeMagnitude))
	{
		StatMagnitudes.Remove(StatTag);
	}
	else
	{
		StatMagnitudes.FindOrAdd(StatTag) = SafeMagnitude;
	}
}
}

bool UEquipmentComponent::ApplyAndStoreWeaponStats(
	const FEquippedItemStatSnapshot& PendingStatSnapshot)
{
	bool bChanged = false;
	return SetAppliedStatSnapshot(
		CachedASC,
		CurrentWeaponStatSnapshot,
		PendingStatSnapshot,
		bChanged);
}

void UEquipmentComponent::ApplyCurrentWeaponTagEffect(
	UPdAbilitySystemComponent* AbilitySystemComponent,
	const UItemDefinition* ItemDefinition)
{
	if (!HasEquipmentAuthority() || !AbilitySystemComponent)
	{
		return;
	}

	if (!EquippedItemEffectClass || !ItemDefinition || !ItemDefinition->IdTag.IsValid())
	{

		return;
	}
	if (CurrentWeaponTagEffectHandle.IsValid())
	{
		return;
	}

	FGameplayEffectContextHandle EffectContext =
		AbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(ItemDefinition);

	FGameplayEffectSpecHandle SpecHandle =
		AbilitySystemComponent->MakeOutgoingSpec(
			EquippedItemEffectClass,
			1.f,
			EffectContext);
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{

		return;
	}

	SpecHandle.Data->DynamicGrantedTags.AddTag(ItemDefinition->IdTag);
	const FActiveGameplayEffectHandle EffectHandle =
		AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(
			*SpecHandle.Data.Get());
	if (EffectHandle.WasSuccessfullyApplied())
	{
		CurrentWeaponTagEffectHandle = EffectHandle;
	}
}

bool UEquipmentComponent::ApplyEquipAbilityCooldown()
{
	if (!HasEquipmentAuthority())
	{
		return false;
	}

	RefreshCachedReferences();
	if (!CachedASC)
	{
		return false;
	}

	if (CachedASC->HasMatchingGameplayTag(LabGameplayTags::Cooldown_EquipWeapon))
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

	FGameplayEffectContextHandle EffectContext = CachedASC->MakeEffectContext();
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

	FGameplayEffectSpecHandle CooldownSpec = CachedASC->MakeOutgoingSpec(
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
	return CachedASC->ApplyGameplayEffectSpecToSelf(
		*CooldownSpec.Data.Get()).WasSuccessfullyApplied();
}

void UEquipmentComponent::RemoveCurrentWeaponTagEffect(
	UPdAbilitySystemComponent* AbilitySystemComponent,
	const UItemDefinition* ItemDefinition)
{
	if (!HasEquipmentAuthority())
	{
		return;
	}

	if (!AbilitySystemComponent)
	{
		CurrentWeaponTagEffectHandle.Invalidate();
		return;
	}

	if (CurrentWeaponTagEffectHandle.IsValid())
	{
		const bool bRemovedByHandle =
			AbilitySystemComponent->RemoveActiveGameplayEffect(
				CurrentWeaponTagEffectHandle);
		CurrentWeaponTagEffectHandle.Invalidate();
		if (bRemovedByHandle)
		{
			return;
		}
	}

	if (ItemDefinition && ItemDefinition->IdTag.IsValid())
	{
		FGameplayTagContainer GrantedTags;
		GrantedTags.AddTag(ItemDefinition->IdTag);
		AbilitySystemComponent->RemoveActiveEffectsWithGrantedTags(GrantedTags);
	}
}

bool UEquipmentComponent::RemoveCurrentWeaponStats()
{
	if (!CurrentWeaponStatSnapshot.HasAnyMagnitude())
	{
		return true;
	}

	FEquippedItemStatSnapshot EmptySnapshot;
	bool bChanged = false;
	if (!SetAppliedStatSnapshot(
			CachedASC,
			CurrentWeaponStatSnapshot,
			EmptySnapshot,
			bChanged))
	{
		UE_LOG(
			EquipmentComponentLog,
			Error,
			TEXT("Failed to remove the current weapon stat snapshot for %s. The weapon state was preserved to prevent stat accumulation."),
			*GetNameSafe(GetOwner()));
		return false;
	}

	return true;
}

void UEquipmentComponent::HandleEquipmentSlotsChanged()
{
	bEquipmentStatsInitialized = false;
	RefreshCachedReferences();
	if (!HasEquipmentAuthority())
	{
		OnEquipmentStatsChanged.Broadcast();
	}
}

void UEquipmentComponent::HandleInventoryChanged()
{
	if (HasEquipmentAuthority())
	{
		bEquipmentStatsInitialized = false;
		RefreshCachedReferences();
		return;
	}

	OnEquipmentStatsChanged.Broadcast();
}

bool UEquipmentComponent::BuildEquippedItemsStatSnapshot(
	FEquippedItemStatSnapshot& OutSnapshot) const
{
	OutSnapshot.Reset();
	const UInventoryComponent* Inventory = CachedInventory.Get();
	if (!Inventory)
	{
		return false;
	}

	const UProjectTagConfig* TagConfig = UProjectTagConfig::Get(this);
	TArray<FGameplayTag> EquipmentSlotTags;
	TagConfig->GetItemEquipmentSlotTags(EquipmentSlotTags);

	for (const FGameplayTag& EquipmentSlotTag : EquipmentSlotTags)
	{
		const UItemInstance* ItemInstance =
			Inventory->GetEquipmentSlotItem(EquipmentSlotTag);
		const UItemDefinition* ItemDefinition = IsValid(ItemInstance)
			? ItemInstance->ItemDefinition.Get()
			: nullptr;
		if (!ItemDefinition)
		{
			continue;
		}

		for (const TPair<FGameplayTag, float>& Pair :
			ItemDefinition->Map_Stat_Magnitude)
		{
			AddEquipmentStatMagnitude(
				OutSnapshot.BaseStatMagnitudes,
				Pair.Key,
				Pair.Value);
		}
		for (const TPair<FGameplayTag, float>& Pair :
			ItemInstance->Map_EnhancedStat_Magnitude)
		{
			AddEquipmentStatMagnitude(
				OutSnapshot.EnhancedStatMagnitudes,
				Pair.Key,
				Pair.Value);
		}

		TMap<FGameplayTag, float> UpgradeBonusMagnitudes;
		ItemInstance->BuildUpgradeBonusStatMagnitudes(UpgradeBonusMagnitudes);
		for (const TPair<FGameplayTag, float>& Pair : UpgradeBonusMagnitudes)
		{
			AddEquipmentStatMagnitude(
				OutSnapshot.EnhancedStatMagnitudes,
				Pair.Key,
				Pair.Value);
		}
	}
	return true;
}

bool UEquipmentComponent::BuildCurrentWeaponStatSnapshot(
	FEquippedItemStatSnapshot& OutSnapshot) const
{
	OutSnapshot.Reset();
	if (!CurrentWeaponId.IsValid() && !CurrentWeaponDefinition)
	{
		return true;
	}

	if (const UItemInstance* WeaponInstance =
		FindOwnedItemInstanceById(CurrentWeaponId))
	{
		return BuildItemStatSnapshot(WeaponInstance, OutSnapshot);
	}

	return CurrentWeaponDefinition
		? BuildItemDefinitionStatSnapshot(
			CurrentWeaponDefinition,
			OutSnapshot)
		: false;
}

bool UEquipmentComponent::RefreshEquipmentStats()
{
	if (bRefreshingEquipmentStats
		|| !HasEquipmentAuthority()
		|| !CachedASC)
	{
		return false;
	}

	TGuardValue<bool> RefreshGuard(bRefreshingEquipmentStats, true);
	FEquippedItemStatSnapshot DesiredEquippedItemsSnapshot;
	FEquippedItemStatSnapshot DesiredWeaponSnapshot;
	if (!BuildEquippedItemsStatSnapshot(DesiredEquippedItemsSnapshot)
		|| !BuildCurrentWeaponStatSnapshot(DesiredWeaponSnapshot))
	{
		return false;
	}

	bool bEquippedItemsChanged = false;
	if (!SetAppliedStatSnapshot(
			CachedASC,
			EquippedItemsStatSnapshot,
			DesiredEquippedItemsSnapshot,
			bEquippedItemsChanged))
	{
		UE_LOG(
			EquipmentComponentLog,
			Error,
			TEXT("Failed to synchronize equipped item stats for %s."),
			*GetNameSafe(GetOwner()));
		return false;
	}

	bool bWeaponChanged = false;
	if (!SetAppliedStatSnapshot(
			CachedASC,
			CurrentWeaponStatSnapshot,
			DesiredWeaponSnapshot,
			bWeaponChanged))
	{
		UE_LOG(
			EquipmentComponentLog,
			Error,
			TEXT("Failed to synchronize current weapon stats for %s."),
			*GetNameSafe(GetOwner()));
		return false;
	}

	bEquipmentStatsInitialized = true;
	if (bEquippedItemsChanged || bWeaponChanged)
	{
		NotifyEquipmentStatsChanged();
	}
	return true;
}

bool UEquipmentComponent::ClearAppliedEquipmentState(
	UPdAbilitySystemComponent* AbilitySystemComponent)
{
	RemoveCurrentWeaponTagEffect(
		AbilitySystemComponent,
		CurrentWeaponDefinition);

	FEquippedItemStatSnapshot EmptySnapshot;
	bool bWeaponChanged = false;
	const bool bRemovedWeaponStats = SetAppliedStatSnapshot(
		AbilitySystemComponent,
		CurrentWeaponStatSnapshot,
		EmptySnapshot,
		bWeaponChanged);

	bool bEquippedItemsChanged = false;
	const bool bRemovedEquippedItemStats = SetAppliedStatSnapshot(
		AbilitySystemComponent,
		EquippedItemsStatSnapshot,
		EmptySnapshot,
		bEquippedItemsChanged);

	bEquipmentStatsInitialized = false;
	return bRemovedWeaponStats && bRemovedEquippedItemStats;
}

void UEquipmentComponent::NotifyEquipmentStatsChanged()
{
	if (CachedASC)
	{
		CachedASC->OnAbilitiesChangedNative.Broadcast();
	}
	if (const ACharacterBase* CharacterOwner = CachedOwner.Get())
	{
		if (UCharacterAbilityRuntimeComponent* AbilityRuntime =
			CharacterOwner->GetCharacterAbilityRuntimeComponent())
		{
			AbilityRuntime->ApplyMovementSpeedFromAttribute();
		}
	}
	OnEquipmentStatsChanged.Broadcast();
}

void UEquipmentComponent::UnbindEquipmentSlotsChanged()
{
	if (CachedInventory && EquipmentSlotsChangedDelegateHandle.IsValid())
	{
		CachedInventory->OnEquipmentSlotsChanged.Remove(
			EquipmentSlotsChangedDelegateHandle);
	}
	EquipmentSlotsChangedDelegateHandle.Reset();
}

void UEquipmentComponent::UnbindInventoryChanged()
{
	if (CachedInventory && InventoryChangedDelegateHandle.IsValid())
	{
		CachedInventory->OnInventoryChanged.Remove(
			InventoryChangedDelegateHandle);
	}
	InventoryChangedDelegateHandle.Reset();
}

const UItemDefinition* UEquipmentComponent::GetCurrentWeaponDefinition() const
{
	if (CurrentWeaponDefinition)
	{
		return CurrentWeaponDefinition.Get();
	}

	if (CurrentWeaponId.IsValid())
	{
		if (const UItemInstance* EquippedItemInstance = FindOwnedItemInstanceById(CurrentWeaponId))
		{
			return EquippedItemInstance->ItemDefinition.Get();
		}
	}

	return nullptr;
}

float UEquipmentComponent::GetCurrentWeaponStatMagnitude(
	const FGameplayTag StatTag) const
{
	if (!StatTag.IsValid())
	{
		return 0.0f;
	}

	if (const UItemInstance* EquippedItemInstance =
		FindOwnedItemInstanceById(CurrentWeaponId))
	{
		return EquippedItemInstance->GetEffectiveStatMagnitude(StatTag);
	}

	const UItemDefinition* ItemDefinition = GetCurrentWeaponDefinition();
	return ItemDefinition
		? ItemDefinition->Map_Stat_Magnitude.FindRef(StatTag)
		: 0.0f;
}

void UEquipmentComponent::GetEquipmentBonusStatMagnitudes(
	TMap<FGameplayTag, float>& OutStatMagnitudes) const
{
	OutStatMagnitudes.Reset();
	const auto AppendSnapshot = [&OutStatMagnitudes](
		const FEquippedItemStatSnapshot& Snapshot)
	{
		const auto AppendMagnitudes = [&OutStatMagnitudes](
			const TMap<FGameplayTag, float>& StatMagnitudes)
		{
			for (const TPair<FGameplayTag, float>& Pair : StatMagnitudes)
			{
				AddEquipmentStatMagnitude(OutStatMagnitudes, Pair.Key, Pair.Value);
			}
		};

		AppendMagnitudes(Snapshot.BaseStatMagnitudes);
		AppendMagnitudes(Snapshot.EnhancedStatMagnitudes);
		AppendMagnitudes(Snapshot.NonAttributeStatMagnitudes);
	};

	FEquippedItemStatSnapshot EquippedItemsSnapshot;
	if (BuildEquippedItemsStatSnapshot(EquippedItemsSnapshot))
	{
		AppendSnapshot(EquippedItemsSnapshot);
	}

	FEquippedItemStatSnapshot WeaponSnapshot;
	if (BuildCurrentWeaponStatSnapshot(WeaponSnapshot))
	{
		AppendSnapshot(WeaponSnapshot);
	}
}

bool UEquipmentComponent::BuildItemStatSnapshot(const UItemInstance* ItemInstance, FEquippedItemStatSnapshot& OutSnapshot) const
{
	const UItemDefinition* ItemDefinition = ItemInstance ? ItemInstance->ItemDefinition.Get() : nullptr;
	if (!BuildItemDefinitionStatSnapshot(ItemDefinition, OutSnapshot))
	{
		return false;
	}

	for (const TPair<FGameplayTag, float>& Pair : ItemInstance->Map_EnhancedStat_Magnitude)
	{
		if (!Pair.Key.IsValid()
			|| FMath::IsNearlyZero(Pair.Value))
		{
			continue;
		}

		TMap<FGameplayTag, float>& TargetMagnitudes =
			Pair.Key.MatchesTagExact(LabGameplayTags::Status_Offense_Strength)
				? OutSnapshot.NonAttributeStatMagnitudes
				: OutSnapshot.EnhancedStatMagnitudes;
		AddEquipmentStatMagnitude(TargetMagnitudes, Pair.Key, Pair.Value);
	}

	TMap<FGameplayTag, float> UpgradeBonusMagnitudes;
	ItemInstance->BuildUpgradeBonusStatMagnitudes(UpgradeBonusMagnitudes);
	for (const TPair<FGameplayTag, float>& Pair : UpgradeBonusMagnitudes)
	{
		if (!Pair.Key.IsValid()
			|| FMath::IsNearlyZero(Pair.Value))
		{
			continue;
		}

		TMap<FGameplayTag, float>& TargetMagnitudes =
			Pair.Key.MatchesTagExact(LabGameplayTags::Status_Offense_Strength)
				? OutSnapshot.NonAttributeStatMagnitudes
				: OutSnapshot.EnhancedStatMagnitudes;
		AddEquipmentStatMagnitude(TargetMagnitudes, Pair.Key, Pair.Value);
	}

	return true;
}

bool UEquipmentComponent::BuildItemDefinitionStatSnapshot(const UItemDefinition* ItemDefinition, FEquippedItemStatSnapshot& OutSnapshot) const
{
	OutSnapshot.Reset();

	if (!ItemDefinition)
	{
		return false;
	}

	for (const TPair<FGameplayTag, float>& Pair : ItemDefinition->Map_Stat_Magnitude)
	{
		if (!Pair.Key.IsValid()
			|| FMath::IsNearlyZero(Pair.Value))
		{
			continue;
		}

		TMap<FGameplayTag, float>& TargetMagnitudes =
			Pair.Key.MatchesTagExact(LabGameplayTags::Status_Offense_Strength)
				? OutSnapshot.NonAttributeStatMagnitudes
				: OutSnapshot.BaseStatMagnitudes;
		AddEquipmentStatMagnitude(TargetMagnitudes, Pair.Key, Pair.Value);
	}

	return true;
}

bool UEquipmentComponent::ApplyItemStatSnapshot(
	UPdAbilitySystemComponent* AbilitySystemComponent,
	const FEquippedItemStatSnapshot& StatSnapshot,
	const float MagnitudeScale) const
{
	if (!StatSnapshot.HasAnyMagnitude())
	{
		return true;
	}

	if (!AbilitySystemComponent)
	{
		return false;
	}

	TMap<FGameplayTag, float> CombinedStatMagnitudes;
	const auto AddMagnitudeMap = [MagnitudeScale, &CombinedStatMagnitudes](const TMap<FGameplayTag, float>& StatMagnitudes)
	{
		for (const TPair<FGameplayTag, float>& Pair : StatMagnitudes)
		{
			const float ScaledMagnitude = Pair.Value * MagnitudeScale;
			if (!Pair.Key.IsValid() || FMath::IsNearlyZero(ScaledMagnitude))
			{
				continue;
			}

			CombinedStatMagnitudes.FindOrAdd(Pair.Key) += ScaledMagnitude;
		}
	};

	AddMagnitudeMap(StatSnapshot.BaseStatMagnitudes);
	AddMagnitudeMap(StatSnapshot.EnhancedStatMagnitudes);
	if (CombinedStatMagnitudes.IsEmpty())
	{
		return true;
	}

	if (!EquipmentStatGameplayEffectClass)
	{

		return false;
	}

	struct FMaxResourceState
	{
		FGameplayTag CurrentStatTag;
		FGameplayAttribute MaxAttribute;
		FGameplayAttribute CurrentAttribute;
		float OldMaxValue = 0.0f;
		float OldCurrentValue = 0.0f;
	};

	TArray<FMaxResourceState, TInlineAllocator<4>> MaxResourceStates;
	const auto CaptureMaxResourceState = [
		AbilitySystemComponent,
		&CombinedStatMagnitudes,
		&MaxResourceStates](
		const FGameplayTag MaxStatTag,
		const FGameplayTag CurrentStatTag,
		const FGameplayAttribute& MaxAttribute,
		const FGameplayAttribute& CurrentAttribute)
	{
		const float* MaxMagnitude = CombinedStatMagnitudes.Find(MaxStatTag);
		if (!MaxMagnitude || FMath::IsNearlyZero(*MaxMagnitude))
		{
			return;
		}

		FMaxResourceState& State = MaxResourceStates.AddDefaulted_GetRef();
		State.CurrentStatTag = CurrentStatTag;
		State.MaxAttribute = MaxAttribute;
		State.CurrentAttribute = CurrentAttribute;
		State.OldMaxValue =
			AbilitySystemComponent->GetNumericAttribute(MaxAttribute);
		State.OldCurrentValue =
			AbilitySystemComponent->GetNumericAttribute(CurrentAttribute);
	};

	CaptureMaxResourceState(
		LabGameplayTags::Status_Resource_MaxHealth,
		LabGameplayTags::Status_Resource_Health,
		UBasicAttributeSet::GetMaxHealthAttribute(),
		UBasicAttributeSet::GetHealthAttribute());
	CaptureMaxResourceState(
		LabGameplayTags::Status_Defense_MaxShield,
		LabGameplayTags::Status_Defense_Shield,
		UBasicAttributeSet::GetMaxShieldAttribute(),
		UBasicAttributeSet::GetShieldAttribute());
	CaptureMaxResourceState(
		LabGameplayTags::Status_Resource_MaxMana,
		LabGameplayTags::Status_Resource_Mana,
		UBasicAttributeSet::GetMaxManaAttribute(),
		UBasicAttributeSet::GetManaAttribute());
	CaptureMaxResourceState(
		LabGameplayTags::Status_Resource_MaxStamina,
		LabGameplayTags::Status_Resource_Stamina,
		UBasicAttributeSet::GetMaxStaminaAttribute(),
		UBasicAttributeSet::GetStaminaAttribute());

	if (!AbilitySystemComponent->ApplyStatUpEffectByTags(
		EquipmentStatGameplayEffectClass,
		CombinedStatMagnitudes,
		EEnum_Operation::Add))
	{
		return false;
	}

	for (const FMaxResourceState& State : MaxResourceStates)
	{
		const float NewMaxValue = FMath::Max(
			AbilitySystemComponent->GetNumericAttribute(State.MaxAttribute),
			0.0f);
		const bool bWasEffectivelyFull =
			State.OldMaxValue <= UE_KINDA_SMALL_NUMBER
			|| State.OldCurrentValue >= State.OldMaxValue - 1.0f;
		const float RatioPreservedValue = bWasEffectivelyFull
			? NewMaxValue
			: State.OldCurrentValue
				* (NewMaxValue / State.OldMaxValue);
		const float DirectCurrentStatChange =
			CombinedStatMagnitudes.FindRef(State.CurrentStatTag);
		const float NewCurrentValue = FMath::Clamp(
			RatioPreservedValue + DirectCurrentStatChange,
			0.0f,
			NewMaxValue);
		if (!FMath::IsNearlyEqual(
				AbilitySystemComponent->GetNumericAttribute(
					State.CurrentAttribute),
				NewCurrentValue)
			&& !AbilitySystemComponent->ApplyAttributeDefaultValue(
				State.CurrentAttribute,
				NewCurrentValue))
		{
			UE_LOG(
				EquipmentComponentLog,
				Warning,
				TEXT("Failed to preserve the current resource ratio after an equipment max stat change for %s."),
				*GetNameSafe(GetOwner()));
		}
	}

	return true;
}

bool UEquipmentComponent::SetAppliedStatSnapshot(
	UPdAbilitySystemComponent* AbilitySystemComponent,
	FEquippedItemStatSnapshot& AppliedSnapshot,
	const FEquippedItemStatSnapshot& DesiredSnapshot,
	bool& bOutChanged) const
{
	bOutChanged = false;
	if (AreStatSnapshotsEqual(AppliedSnapshot, DesiredSnapshot))
	{
		return true;
	}

	if (!AbilitySystemComponent)
	{
		return false;
	}

	const FEquippedItemStatSnapshot PreviousSnapshot = AppliedSnapshot;
	if (PreviousSnapshot.HasAnyMagnitude()
		&& !ApplyItemStatSnapshot(
			AbilitySystemComponent,
			PreviousSnapshot,
			-1.0f))
	{
		return false;
	}
	AppliedSnapshot.Reset();

	if (DesiredSnapshot.HasAnyMagnitude()
		&& !ApplyItemStatSnapshot(
			AbilitySystemComponent,
			DesiredSnapshot,
			1.0f))
	{
		if (PreviousSnapshot.HasAnyMagnitude()
			&& ApplyItemStatSnapshot(
				AbilitySystemComponent,
				PreviousSnapshot,
				1.0f))
		{
			AppliedSnapshot = PreviousSnapshot;
		}
		else if (PreviousSnapshot.HasAnyMagnitude())
		{
			UE_LOG(
				EquipmentComponentLog,
				Error,
				TEXT("Failed to restore the previous equipment stat snapshot for %s."),
				*GetNameSafe(GetOwner()));
		}
		return false;
	}

	AppliedSnapshot = DesiredSnapshot;
	bOutChanged = true;
	return true;
}

UItemInstance* UEquipmentComponent::FindOwnedItemInstanceById(FGuid ItemId) const
{
	// =================================================================================================================

	if (!ItemId.IsValid())
	{
		return nullptr;
	}

	UInventoryComponent* InventoryComponent = CachedInventory.Get();
	if (!InventoryComponent)
	{
		const ACharacterBase* CharacterOwner = CachedOwner.Get();
		if (!CharacterOwner)
		{
			CharacterOwner = Cast<ACharacterBase>(GetOwner());
		}

		const APdPlayerState* PdPlayerState = CharacterOwner ? Cast<APdPlayerState>(CharacterOwner->GetPlayerState()) : nullptr;
		InventoryComponent = PdPlayerState ? PdPlayerState->GetInventoryComponent() : nullptr;
	}

	if (!InventoryComponent)
	{
		return nullptr;
	}

	return InventoryComponent->FindItemInstanceById(ItemId);
}
