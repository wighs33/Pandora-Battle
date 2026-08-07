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

void UEquipmentComponent::ApplyAndStoreWeaponStats(const UItemDefinition* ItemDefinition, FEquippedItemStatSnapshot& PendingStatSnapshot)
{
	if (ApplyItemStatSnapshot(PendingStatSnapshot, 1.f)
		&& PendingStatSnapshot.HasAnyMagnitude())
	{
		CurrentWeaponStatSnapshot = MoveTemp(PendingStatSnapshot);
	}
}

void UEquipmentComponent::ApplyCurrentWeaponTagEffect(const UItemDefinition* ItemDefinition)
{
	if (!HasEquipmentAuthority())
	{
		return;
	}

	RefreshCachedReferences();

	if (!CachedASC)
	{

		return;
	}

	if (!EquippedItemEffectClass || !ItemDefinition || !ItemDefinition->IdTag.IsValid())
	{

		return;
	}

	FGameplayEffectContextHandle EffectContext = CachedASC->MakeEffectContext();
	EffectContext.AddSourceObject(ItemDefinition);

	FGameplayEffectSpecHandle SpecHandle = CachedASC->MakeOutgoingSpec(EquippedItemEffectClass, 1.f, EffectContext);
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{

		return;
	}

	SpecHandle.Data->DynamicGrantedTags.AddTag(ItemDefinition->IdTag);
	const FActiveGameplayEffectHandle EffectHandle = CachedASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
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

void UEquipmentComponent::RemoveCurrentWeaponTagEffect(const UItemDefinition* ItemDefinition)
{
	if (!HasEquipmentAuthority())
	{
		return;
	}

	RefreshCachedReferences();

	if (!CachedASC)
	{
		return;
	}

	if (CurrentWeaponTagEffectHandle.IsValid())
	{
		CachedASC->RemoveActiveGameplayEffect(CurrentWeaponTagEffectHandle);
		CurrentWeaponTagEffectHandle.Invalidate();
		return;
	}

	if (ItemDefinition && ItemDefinition->IdTag.IsValid())
	{
		FGameplayTagContainer GrantedTags;
		GrantedTags.AddTag(ItemDefinition->IdTag);
		CachedASC->RemoveActiveEffectsWithGrantedTags(GrantedTags);
	}
}

bool UEquipmentComponent::RemoveCurrentWeaponStats()
{
	if (!CurrentWeaponStatSnapshot.HasAnyMagnitude())
	{
		return true;
	}

	RefreshCachedReferences();
	if (!ApplyItemStatSnapshot(CurrentWeaponStatSnapshot, -1.f))
	{
		UE_LOG(
			EquipmentComponentLog,
			Error,
			TEXT("Failed to remove the current weapon stat snapshot for %s. The weapon state was preserved to prevent stat accumulation."),
			*GetNameSafe(GetOwner()));
		return false;
	}

	CurrentWeaponStatSnapshot.Reset();
	return true;
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
			|| Pair.Key.MatchesTagExact(LabGameplayTags::Status_Offense_Strength)
			|| FMath::IsNearlyZero(Pair.Value))
		{
			continue;
		}

		OutSnapshot.EnhancedStatMagnitudes.FindOrAdd(Pair.Key) += Pair.Value;
	}

	TMap<FGameplayTag, float> UpgradeBonusMagnitudes;
	ItemInstance->BuildUpgradeBonusStatMagnitudes(UpgradeBonusMagnitudes);
	for (const TPair<FGameplayTag, float>& Pair : UpgradeBonusMagnitudes)
	{
		if (!Pair.Key.IsValid()
			|| Pair.Key.MatchesTagExact(LabGameplayTags::Status_Offense_Strength)
			|| FMath::IsNearlyZero(Pair.Value))
		{
			continue;
		}

		OutSnapshot.EnhancedStatMagnitudes.FindOrAdd(Pair.Key) += Pair.Value;
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
			|| Pair.Key.MatchesTagExact(LabGameplayTags::Status_Offense_Strength)
			|| FMath::IsNearlyZero(Pair.Value))
		{
			continue;
		}

		OutSnapshot.BaseStatMagnitudes.FindOrAdd(Pair.Key) += Pair.Value;
	}

	return true;
}

bool UEquipmentComponent::ApplyItemStatSnapshot(const FEquippedItemStatSnapshot& StatSnapshot, float MagnitudeScale) const
{
	if (!StatSnapshot.HasAnyMagnitude())
	{
		return true;
	}

	UPdAbilitySystemComponent* ASC = CachedASC.Get();
	if (!ASC)
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

	return ASC->ApplyStatUpEffectByTags(
		EquipmentStatGameplayEffectClass,
		CombinedStatMagnitudes,
		EEnum_Operation::Add);
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
