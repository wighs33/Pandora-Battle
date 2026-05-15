#include "Common/ProjectTagConfig.h"

#include "Engine/World.h"
#include "GameFeature/ProjectTagConfigWorldSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProjectTagConfig)

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
	SkinHatEquipTypeTag = LabGameplayTags::Skin_Cosmetics_Hat;
	SkinTopEquipTypeTag = LabGameplayTags::Skin_Cosmetics_Top;
	SkinBottomEquipTypeTag = LabGameplayTags::Skin_Cosmetics_Bottom;
	SkinShoesEquipTypeTag = LabGameplayTags::Skin_Cosmetics_Shoes;
	SkinHairEquipTypeTag = LabGameplayTags::Skin_Cosmetics_Hair;
	SkinFaceEquipTypeTag = LabGameplayTags::Skin_Cosmetics_Face;
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
	StatusArcaneTag = LabGameplayTags::Status_Offense_Arcane;
	StatusToughnessTag = LabGameplayTags::Status_Defense_Toughness;
	StatusRecoveryTag = LabGameplayTags::Status_Defense_Recovery;
	StatusMagicResistanceTag = LabGameplayTags::Status_Defense_MagicResistance;
	StatusImmunityTag = LabGameplayTags::Status_Resistance_Immunity;
	StatusFortitudeTag = LabGameplayTags::Status_Resistance_Fortitude;
	StatusSanityTag = LabGameplayTags::Status_Resistance_Sanity;
	StatusFirstPandoraTag = LabGameplayTags::Status_PandoraForce_FirstPandora;
	StatusSecondPandoraTag = LabGameplayTags::Status_PandoraForce_SecondPandora;
	StatusThirdPandoraTag = LabGameplayTags::Status_PandoraForce_ThirdPandora;
	StatusMaxHealthTag = LabGameplayTags::Status_Resource_MaxHealth;
	StatusMaxManaTag = LabGameplayTags::Status_Resource_MaxMana;
	StatusMaxStaminaTag = LabGameplayTags::Status_Resource_MaxStamina;
	StatusAttackSpeedTag = LabGameplayTags::Status_Agility_AttackSpeed;
	StatusMovementSpeedTag = LabGameplayTags::Status_Agility_MovementSpeed;
	StatusCriticalChanceTag = LabGameplayTags::Status_Agility_CriticalChance;

	CombatAttackAbilityTag = LabGameplayTags::Action_Attack;
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
	return FPrimaryAssetId(TEXT("ProjectTagConfig"), GetFName());
}

const UProjectTagConfig* UProjectTagConfig::Get(const UObject* WorldContextObject)
{
	if (WorldContextObject)
	{
		if (const UWorld* World = WorldContextObject->GetWorld())
		{
			const UProjectTagConfigWorldSubsystem* Subsystem = World->GetSubsystem<UProjectTagConfigWorldSubsystem>();
			if (const UProjectTagConfig* ProjectTagConfig = Subsystem ? Subsystem->GetProjectTagConfig() : nullptr)
			{
				return ProjectTagConfig;
			}
		}
	}

	return GetDefaultConfig();
}

const UProjectTagConfig* UProjectTagConfig::GetDefaultConfig()
{
	return GetDefault<UProjectTagConfig>();
}

void UProjectTagConfig::GetItemFilterTypeTags(TArray<FGameplayTag>& OutTags) const
{
	OutTags.Reset();
	AddValidTag(OutTags, GetItemWeaponTypeTag());
	AddValidTag(OutTags, GetItemEquipmentTypeTag());
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
	AddValidTag(OutTags, GetSkinGestureTypeTag());
	AddValidTag(OutTags, GetSkinRidingTypeTag());
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
