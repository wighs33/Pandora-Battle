#include "Definition/Common/ProjectTagConfig.h"

#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Logging/PdLogRateLimiter.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProjectTagConfig)

DEFINE_LOG_CATEGORY_STATIC(LogProjectTagConfig, Log, All);

namespace
{
	constexpr double RequiredConfigLogIntervalSeconds = 30.0;
	FPdLogRateLimiter MissingConfigLogLimiter;
	FPdLogRateLimiter LoadFailedConfigLogLimiter;
	TSharedPtr<FStreamableHandle> PendingConfigLoadHandle;

	const UProjectTagConfig* LoadProjectTagConfigPrimaryAsset()
	{
		UAssetManager* AssetManager = UAssetManager::GetIfInitialized();
		if (!AssetManager)
		{
			// Very early editor/CDO code can run before AssetManager initialization.
			// GetDefaultConfig() provides native defaults until PrimaryAssets are available.
			return nullptr;
		}

		static FPrimaryAssetId ResolvedConfigId;
		if (!ResolvedConfigId.IsValid()
			|| !AssetManager->GetPrimaryAssetPath(ResolvedConfigId).IsValid())
		{
			TArray<FPrimaryAssetId> ConfigIds;
			AssetManager->GetPrimaryAssetIdList(UProjectTagConfig::GetConfigPrimaryAssetType(), ConfigIds);

			if (ConfigIds.IsEmpty())
			{
				uint32 SuppressedCount = 0;
				if (MissingConfigLogLimiter.TryAcquire(
					RequiredConfigLogIntervalSeconds,
					SuppressedCount))
				{
					UE_LOG(
						LogProjectTagConfig,
						Error,
						TEXT("No ProjectTagConfig PrimaryAsset is registered. Expected %s. "
							"SuppressedSinceLast=%u"),
						*UProjectTagConfig::GetPreferredPrimaryAssetId().ToString(),
						SuppressedCount);
				}
				return nullptr;
			}

			const FPrimaryAssetId PreferredId = UProjectTagConfig::GetPreferredPrimaryAssetId();
			const FPrimaryAssetId* PreferredConfig = ConfigIds.FindByPredicate(
				[&PreferredId](const FPrimaryAssetId& ConfigId)
				{
					return ConfigId == PreferredId;
				});

			if (ConfigIds.Num() > 1)
			{
				UE_LOG(
					LogProjectTagConfig,
					Error,
					TEXT("Multiple ProjectTagConfig PrimaryAssets are registered (%d). "
						"Keep exactly one. %s will be used when available."),
					ConfigIds.Num(),
					*PreferredId.ToString());
			}

			if (PreferredConfig)
			{
				ResolvedConfigId = *PreferredConfig;
			}
			else if (ConfigIds.Num() == 1)
			{
				// Supports safely renaming the sole config asset.
				ResolvedConfigId = ConfigIds[0];
			}
			else
			{
				return nullptr;
			}
		}

		if (const UProjectTagConfig* LoadedConfig =
			AssetManager->GetPrimaryAssetObject<UProjectTagConfig>(ResolvedConfigId))
		{
			return LoadedConfig;
		}

		if (!PendingConfigLoadHandle.IsValid()
			|| PendingConfigLoadHandle->HasLoadCompleted())
		{
			PendingConfigLoadHandle =
				AssetManager->LoadPrimaryAsset(ResolvedConfigId);
		}

		const UProjectTagConfig* LoadedConfig =
			AssetManager->GetPrimaryAssetObject<UProjectTagConfig>(ResolvedConfigId);
		if (!LoadedConfig
			&& (!PendingConfigLoadHandle.IsValid()
				|| PendingConfigLoadHandle->HasLoadCompleted()))
		{
			uint32 SuppressedCount = 0;
			if (LoadFailedConfigLogLimiter.TryAcquire(
				RequiredConfigLogIntervalSeconds,
				SuppressedCount))
			{
				UE_LOG(
					LogProjectTagConfig,
					Error,
					TEXT("Failed to load ProjectTagConfig PrimaryAsset %s at %s. "
						"SuppressedSinceLast=%u"),
					*ResolvedConfigId.ToString(),
					*AssetManager->GetPrimaryAssetPath(ResolvedConfigId).ToString(),
					SuppressedCount);
			}
		}

		return LoadedConfig;
	}
}

UProjectTagConfig::UProjectTagConfig()
{
	ItemWeaponTypeTag = LabGameplayTags::Item_Weapon;
	ItemEquipmentTypeTag = LabGameplayTags::Item_Equipment;
	ItemConsumableTypeTag = LabGameplayTags::Item_Consumable;
	ItemValuableTypeTag = LabGameplayTags::Item_Valuable;
	ItemHatEquipTypeTag = LabGameplayTags::Item_Equipment_Hat;
	ItemTopEquipTypeTag = LabGameplayTags::Item_Equipment_Top;
	ItemBottomEquipTypeTag = LabGameplayTags::Item_Equipment_Bottom;
	ItemShoesEquipTypeTag = LabGameplayTags::Item_Equipment_Shoes;
	ItemEarringEquipTypeTag = LabGameplayTags::Item_Equipment_Earring;
	ItemNecklaceEquipTypeTag = LabGameplayTags::Item_Equipment_Necklace;
	ItemRingEquipTypeTag = LabGameplayTags::Item_Equipment_Ring;
	ItemRuneEquipTypeTag = LabGameplayTags::Item_Equipment_Rune;

	PandoraOffensiveTypeTag = LabGameplayTags::Pandora_Offensive;
	PandoraDefensiveTypeTag = LabGameplayTags::Pandora_Defensive;
	PandoraSupportTypeTag = LabGameplayTags::Pandora_Support;
	PandoraSpecialTypeTag = LabGameplayTags::Pandora_Special;

	SkinPandoraTypeTag = LabGameplayTags::Skin_Pandora;
	SkinCosmeticsTypeTag = LabGameplayTags::Skin_Cosmetics;
	SkinGestureTypeTag = LabGameplayTags::Skin_Gesture;
	SkinRidingTypeTag = LabGameplayTags::Skin_Riding;
	SkinPetTypeTag = LabGameplayTags::Skin_Pet;
	SkinHatEquipTypeTag = LabGameplayTags::Skin_Cosmetics_Hat;
	SkinTopEquipTypeTag = LabGameplayTags::Skin_Cosmetics_Top;
	SkinBottomEquipTypeTag = LabGameplayTags::Skin_Cosmetics_Bottom;
	SkinShoesEquipTypeTag = LabGameplayTags::Skin_Cosmetics_Shoes;
	SkinHeadEquipTypeTag = LabGameplayTags::Skin_Cosmetics_Head;
	SkinColorEquipTypeTag = LabGameplayTags::Skin_Cosmetics_SkinColor;
	SkinBackEquipTypeTag = LabGameplayTags::Skin_Cosmetics_Back;
	SkinAuraEquipTypeTag = LabGameplayTags::Skin_Cosmetics_Aura;

	UiProfileLeftTag = LabGameplayTags::UI_Profile;
	UiStatusRightTag = LabGameplayTags::UI_Status;
	UiEquipmentLeftTag = LabGameplayTags::UI_Equipment;
	UiInventoryRightTag = LabGameplayTags::UI_Inventory;
	UiSkinEquipmentLeftTag = LabGameplayTags::UI_SkinEquipment;
	UiSkinInventoryRightTag = LabGameplayTags::UI_SkinInventory;
	UiPandoraEquipmentLeftTag = LabGameplayTags::UI_PandoraEquipment;
	UiPandoraInventoryRightTag = LabGameplayTags::UI_PandoraInventory;

	StatusStrengthTag = LabGameplayTags::Status_Offense_Strength;
	StatusIntelligenceTag = LabGameplayTags::Status_Offense_Intelligence;
	StatusArcaneTag = LabGameplayTags::Status_Agility_Arcane;
	StatusArmorTag = LabGameplayTags::Status_Defense_Armor;
	StatusRecoveryTag = LabGameplayTags::Status_Defense_Recovery;
	StatusFrostbiteTag = LabGameplayTags::Status_Resistance_Frostbite;
	StatusBurnTag = LabGameplayTags::Status_Resistance_Burn;
	StatusElectricShockTag = LabGameplayTags::Status_Resistance_ElectricShock;
	StatusFirstPandoraTag = LabGameplayTags::Status_PandoraForce_FirstPandora;
	StatusSecondPandoraTag = LabGameplayTags::Status_PandoraForce_SecondPandora;
	StatusThirdPandoraTag = LabGameplayTags::Status_PandoraForce_ThirdPandora;
	StatusMaxHealthTag = LabGameplayTags::Status_Resource_MaxHealth;
	StatusMaxShieldTag = LabGameplayTags::Status_Defense_MaxShield;
	StatusMaxManaTag = LabGameplayTags::Status_Resource_MaxMana;
	StatusMaxStaminaTag = LabGameplayTags::Status_Resource_MaxStamina;
	StatusAttackSpeedTag = LabGameplayTags::Status_Agility_AttackSpeed;
	StatusMovementSpeedTag = LabGameplayTags::Status_Agility_MovementSpeed;
	StatusCriticalTag = LabGameplayTags::Status_Offense_Critical;

	CombatAttackAbilityTag = LabGameplayTags::Action_Attack;
	CombatPunchAbilityTag = LabGameplayTags::Action_Punch;
	CombatRangedAttackAbilityTag = LabGameplayTags::Action_RangedAttack;
	CombatWeaponDamageSourceTag = LabGameplayTags::Status_Offense_Strength;
	CombatHitReactAbilityTag = LabGameplayTags::Action_HitReact;

	SetByCallerDamageMagnitudeTag = LabGameplayTags::Data_Damage;
	SetByCallerStatUpOperationTag = LabGameplayTags::Data_StatUp;

	EquipmentEquipAbilityTag = LabGameplayTags::Action_Equip;
	EquipmentUnequipAbilityTag = LabGameplayTags::Action_Unequip;
}

FPrimaryAssetId UProjectTagConfig::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(GetConfigPrimaryAssetType(), GetFName());
}

const UProjectTagConfig* UProjectTagConfig::Get(const UObject*)
{
	return GetDefaultConfig();
}

const UProjectTagConfig* UProjectTagConfig::GetDefaultConfig()
{
	if (const UProjectTagConfig* PrimaryConfig = LoadProjectTagConfigPrimaryAsset())
	{
		return PrimaryConfig;
	}

	return GetDefault<UProjectTagConfig>();
}

const FPrimaryAssetType& UProjectTagConfig::GetConfigPrimaryAssetType()
{
	static const FPrimaryAssetType AssetType(TEXT("ProjectTagConfig"));
	return AssetType;
}

const FPrimaryAssetId& UProjectTagConfig::GetPreferredPrimaryAssetId()
{
	static const FPrimaryAssetId AssetId(GetConfigPrimaryAssetType(), TEXT("DA_ProjectTag"));
	return AssetId;
}

#if WITH_EDITOR
EDataValidationResult UProjectTagConfig::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (Result == EDataValidationResult::NotValidated)
	{
		Result = EDataValidationResult::Valid;
	}

	if (const UAssetManager* AssetManager = UAssetManager::GetIfInitialized())
	{
		TArray<FPrimaryAssetId> ConfigIds;
		AssetManager->GetPrimaryAssetIdList(GetConfigPrimaryAssetType(), ConfigIds);
		if (ConfigIds.Num() != 1)
		{
			Context.AddError(FText::Format(
				NSLOCTEXT(
					"ProjectTagConfig",
					"SinglePrimaryAssetRequired",
					"Exactly one ProjectTagConfig PrimaryAsset must be registered, but {0} were found."),
				FText::AsNumber(ConfigIds.Num())));
			Result = EDataValidationResult::Invalid;
		}
	}

	return Result;
}
#endif

void UProjectTagConfig::GetItemFilterTypeTags(TArray<FGameplayTag>& OutTags) const
{
	OutTags.Reset();
	AddValidTag(OutTags, GetItemWeaponTypeTag());
	AddValidTag(OutTags, GetItemEquipmentTypeTag());
	AddValidTag(OutTags, GetItemHatEquipTypeTag());
	AddValidTag(OutTags, GetItemTopEquipTypeTag());
	AddValidTag(OutTags, GetItemBottomEquipTypeTag());
	AddValidTag(OutTags, GetItemShoesEquipTypeTag());
	AddValidTag(OutTags, GetItemEarringEquipTypeTag());
	AddValidTag(OutTags, GetItemNecklaceEquipTypeTag());
	AddValidTag(OutTags, GetItemRingEquipTypeTag());
	AddValidTag(OutTags, GetItemRuneEquipTypeTag());
	AddValidTag(OutTags, GetItemConsumableTypeTag());
	AddValidTag(OutTags, GetItemValuableTypeTag());
}

void UProjectTagConfig::GetPandoraFilterTypeTags(TArray<FGameplayTag>& OutTags) const
{
	OutTags.Reset();
	AddValidTag(OutTags, GetPandoraOffensiveTypeTag());
	AddValidTag(OutTags, GetPandoraDefensiveTypeTag());
	AddValidTag(OutTags, GetPandoraSupportTypeTag());
	AddValidTag(OutTags, GetPandoraSpecialTypeTag());
}

void UProjectTagConfig::GetSkinFilterTypeTags(TArray<FGameplayTag>& OutTags) const
{
	OutTags.Reset();
	AddValidTag(OutTags, GetSkinPandoraTypeTag());
	AddValidTag(OutTags, GetSkinCosmeticsTypeTag());
	AddValidTag(OutTags, GetSkinHatEquipTypeTag());
	AddValidTag(OutTags, GetSkinTopEquipTypeTag());
	AddValidTag(OutTags, GetSkinBottomEquipTypeTag());
	AddValidTag(OutTags, GetSkinShoesEquipTypeTag());
	AddValidTag(OutTags, GetSkinHeadEquipTypeTag());
	AddValidTag(OutTags, GetSkinColorEquipTypeTag());
	AddValidTag(OutTags, GetSkinBackEquipTypeTag());
	AddValidTag(OutTags, GetSkinAuraEquipTypeTag());
	AddValidTag(OutTags, GetSkinGestureTypeTag());
	AddValidTag(OutTags, GetSkinRidingTypeTag());
	AddValidTag(OutTags, GetSkinPetTypeTag());
}

const FGameplayTag& UProjectTagConfig::ResolveTag(const FGameplayTag& ConfiguredTag, const FGameplayTag& DefaultTag)
{
	return ConfiguredTag.IsValid() ? ConfiguredTag : DefaultTag;
}

void UProjectTagConfig::AddValidTag(TArray<FGameplayTag>& OutTags, const FGameplayTag& Tag)
{
	if (Tag.IsValid())
	{
		OutTags.AddUnique(Tag);
	}
}
