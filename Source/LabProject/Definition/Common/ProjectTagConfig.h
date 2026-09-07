#pragma once

#include "CoreMinimal.h"
#include "Common/LabGameplayTags.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ProjectTagConfig.generated.h"

UCLASS(BlueprintType, Blueprintable, meta = (DisplayName = "Project Tag Config"))
class LABPROJECT_API UProjectTagConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UProjectTagConfig();

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	static const UProjectTagConfig* Get(const UObject* WorldContextObject);
	static const UProjectTagConfig* GetDefaultConfig();
	static const FPrimaryAssetType& GetConfigPrimaryAssetType();
	static const FPrimaryAssetId& GetPreferredPrimaryAssetId();

	const FGameplayTag& GetItemWeaponTypeTag() const { return ResolveTag(ItemWeaponTypeTag, LabGameplayTags::Item_Weapon); }
	const FGameplayTag& GetItemEquipmentTypeTag() const { return ResolveTag(ItemEquipmentTypeTag, LabGameplayTags::Item_Equipment); }
	const FGameplayTag& GetItemConsumableTypeTag() const { return ResolveTag(ItemConsumableTypeTag, LabGameplayTags::Item_Consumable); }
	const FGameplayTag& GetItemValuableTypeTag() const { return ResolveTag(ItemValuableTypeTag, LabGameplayTags::Item_Valuable); }

	const FGameplayTag& GetItemHatEquipTypeTag() const { return ResolveTag(ItemHatEquipTypeTag, LabGameplayTags::Item_Equipment_Hat); }
	const FGameplayTag& GetItemTopEquipTypeTag() const { return ResolveTag(ItemTopEquipTypeTag, LabGameplayTags::Item_Equipment_Top); }
	const FGameplayTag& GetItemBottomEquipTypeTag() const { return ResolveTag(ItemBottomEquipTypeTag, LabGameplayTags::Item_Equipment_Bottom); }
	const FGameplayTag& GetItemShoesEquipTypeTag() const { return ResolveTag(ItemShoesEquipTypeTag, LabGameplayTags::Item_Equipment_Shoes); }
	const FGameplayTag& GetItemEarringEquipTypeTag() const { return ResolveTag(ItemEarringEquipTypeTag, LabGameplayTags::Item_Equipment_Earring); }
	const FGameplayTag& GetItemNecklaceEquipTypeTag() const { return ResolveTag(ItemNecklaceEquipTypeTag, LabGameplayTags::Item_Equipment_Necklace); }
	const FGameplayTag& GetItemRingEquipTypeTag() const { return ResolveTag(ItemRingEquipTypeTag, LabGameplayTags::Item_Equipment_Ring); }
	const FGameplayTag& GetItemRuneEquipTypeTag() const { return ResolveTag(ItemRuneEquipTypeTag, LabGameplayTags::Item_Equipment_Rune); }

	const FGameplayTag& GetPandoraOffensiveTypeTag() const { return ResolveTag(PandoraOffensiveTypeTag, LabGameplayTags::Pandora_Offensive); }
	const FGameplayTag& GetPandoraDefensiveTypeTag() const { return ResolveTag(PandoraDefensiveTypeTag, LabGameplayTags::Pandora_Defensive); }
	const FGameplayTag& GetPandoraSupportTypeTag() const { return ResolveTag(PandoraSupportTypeTag, LabGameplayTags::Pandora_Support); }
	const FGameplayTag& GetPandoraSpecialTypeTag() const { return ResolveTag(PandoraSpecialTypeTag, LabGameplayTags::Pandora_Special); }

	const FGameplayTag& GetSkinPandoraTypeTag() const { return ResolveTag(SkinPandoraTypeTag, LabGameplayTags::Skin_Pandora); }
	const FGameplayTag& GetSkinCosmeticsTypeTag() const { return ResolveTag(SkinCosmeticsTypeTag, LabGameplayTags::Skin_Cosmetics); }
	const FGameplayTag& GetSkinGestureTypeTag() const { return ResolveTag(SkinGestureTypeTag, LabGameplayTags::Skin_Gesture); }
	const FGameplayTag& GetSkinRidingTypeTag() const { return ResolveTag(SkinRidingTypeTag, LabGameplayTags::Skin_Riding); }
	const FGameplayTag& GetSkinPetTypeTag() const { return ResolveTag(SkinPetTypeTag, LabGameplayTags::Skin_Pet); }

	const FGameplayTag& GetSkinHatEquipTypeTag() const { return ResolveTag(SkinHatEquipTypeTag, LabGameplayTags::Skin_Cosmetics_Hat); }
	const FGameplayTag& GetSkinTopEquipTypeTag() const { return ResolveTag(SkinTopEquipTypeTag, LabGameplayTags::Skin_Cosmetics_Top); }
	const FGameplayTag& GetSkinBottomEquipTypeTag() const { return ResolveTag(SkinBottomEquipTypeTag, LabGameplayTags::Skin_Cosmetics_Bottom); }
	const FGameplayTag& GetSkinShoesEquipTypeTag() const { return ResolveTag(SkinShoesEquipTypeTag, LabGameplayTags::Skin_Cosmetics_Shoes); }
	const FGameplayTag& GetSkinHeadEquipTypeTag() const { return ResolveTag(SkinHeadEquipTypeTag, LabGameplayTags::Skin_Cosmetics_Head); }
	const FGameplayTag& GetSkinColorEquipTypeTag() const { return ResolveTag(SkinColorEquipTypeTag, LabGameplayTags::Skin_Cosmetics_SkinColor); }
	const FGameplayTag& GetSkinBackEquipTypeTag() const { return ResolveTag(SkinBackEquipTypeTag, LabGameplayTags::Skin_Cosmetics_Back); }
	const FGameplayTag& GetSkinAuraEquipTypeTag() const { return ResolveTag(SkinAuraEquipTypeTag, LabGameplayTags::Skin_Cosmetics_Aura); }

	const FGameplayTag& GetUiProfileLeftTag() const { return ResolveTag(UiProfileLeftTag, LabGameplayTags::UI_Profile); }
	const FGameplayTag& GetUiStatusRightTag() const { return ResolveTag(UiStatusRightTag, LabGameplayTags::UI_Status); }
	const FGameplayTag& GetUiEquipmentLeftTag() const { return ResolveTag(UiEquipmentLeftTag, LabGameplayTags::UI_Equipment); }
	const FGameplayTag& GetUiInventoryRightTag() const { return ResolveTag(UiInventoryRightTag, LabGameplayTags::UI_Inventory); }
	const FGameplayTag& GetUiSkinEquipmentLeftTag() const { return ResolveTag(UiSkinEquipmentLeftTag, LabGameplayTags::UI_SkinEquipment); }
	const FGameplayTag& GetUiSkinInventoryRightTag() const { return ResolveTag(UiSkinInventoryRightTag, LabGameplayTags::UI_SkinInventory); }
	const FGameplayTag& GetUiPandoraEquipmentLeftTag() const { return ResolveTag(UiPandoraEquipmentLeftTag, LabGameplayTags::UI_PandoraEquipment); }
	const FGameplayTag& GetUiPandoraInventoryRightTag() const { return ResolveTag(UiPandoraInventoryRightTag, LabGameplayTags::UI_PandoraInventory); }

	const FGameplayTag& GetStatusStrengthTag() const { return ResolveTag(StatusStrengthTag, LabGameplayTags::Status_Offense_Strength); }
	const FGameplayTag& GetStatusIntelligenceTag() const { return ResolveTag(StatusIntelligenceTag, LabGameplayTags::Status_Offense_Intelligence); }
	const FGameplayTag& GetStatusArcaneTag() const { return ResolveTag(StatusArcaneTag, LabGameplayTags::Status_Agility_Arcane); }
	const FGameplayTag& GetStatusArmorTag() const { return ResolveTag(StatusArmorTag, LabGameplayTags::Status_Defense_Armor); }
	const FGameplayTag& GetStatusRecoveryTag() const { return ResolveTag(StatusRecoveryTag, LabGameplayTags::Status_Defense_Recovery); }
	const FGameplayTag& GetStatusFrostbiteTag() const { return ResolveTag(StatusFrostbiteTag, LabGameplayTags::Status_Resistance_Frostbite); }
	const FGameplayTag& GetStatusBurnTag() const { return ResolveTag(StatusBurnTag, LabGameplayTags::Status_Resistance_Burn); }
	const FGameplayTag& GetStatusElectricShockTag() const { return ResolveTag(StatusElectricShockTag, LabGameplayTags::Status_Resistance_ElectricShock); }
	const FGameplayTag& GetStatusFirstPandoraTag() const { return ResolveTag(StatusFirstPandoraTag, LabGameplayTags::Status_PandoraForce_FirstPandora); }
	const FGameplayTag& GetStatusSecondPandoraTag() const { return ResolveTag(StatusSecondPandoraTag, LabGameplayTags::Status_PandoraForce_SecondPandora); }
	const FGameplayTag& GetStatusThirdPandoraTag() const { return ResolveTag(StatusThirdPandoraTag, LabGameplayTags::Status_PandoraForce_ThirdPandora); }
	const FGameplayTag& GetStatusMaxHealthTag() const { return ResolveTag(StatusMaxHealthTag, LabGameplayTags::Status_Resource_MaxHealth); }
	const FGameplayTag& GetStatusMaxShieldTag() const { return ResolveTag(StatusMaxShieldTag, LabGameplayTags::Status_Defense_MaxShield); }
	const FGameplayTag& GetStatusMaxManaTag() const { return ResolveTag(StatusMaxManaTag, LabGameplayTags::Status_Resource_MaxMana); }
	const FGameplayTag& GetStatusMaxStaminaTag() const { return ResolveTag(StatusMaxStaminaTag, LabGameplayTags::Status_Resource_MaxStamina); }
	const FGameplayTag& GetStatusAttackSpeedTag() const { return ResolveTag(StatusAttackSpeedTag, LabGameplayTags::Status_Agility_AttackSpeed); }
	const FGameplayTag& GetStatusMovementSpeedTag() const { return ResolveTag(StatusMovementSpeedTag, LabGameplayTags::Status_Agility_MovementSpeed); }
	const FGameplayTag& GetStatusCriticalTag() const { return ResolveTag(StatusCriticalTag, LabGameplayTags::Status_Offense_Critical); }

	const FGameplayTag& GetCombatAttackAbilityTag() const { return ResolveTag(CombatAttackAbilityTag, LabGameplayTags::Action_Attack); }
	const FGameplayTag& GetCombatPunchAbilityTag() const { return ResolveTag(CombatPunchAbilityTag, LabGameplayTags::Action_Punch); }
	const FGameplayTag& GetCombatRangedAttackAbilityTag() const { return ResolveTag(CombatRangedAttackAbilityTag, LabGameplayTags::Action_RangedAttack); }
	const FGameplayTag& GetCombatWeaponDamageSourceTag() const { return ResolveTag(CombatWeaponDamageSourceTag, LabGameplayTags::Status_Offense_Strength); }
	const FGameplayTag& GetCombatHitReactAbilityTag() const { return ResolveTag(CombatHitReactAbilityTag, LabGameplayTags::Action_HitReact); }

	const FGameplayTag& GetSetByCallerDamageMagnitudeTag() const { return ResolveTag(SetByCallerDamageMagnitudeTag, LabGameplayTags::Data_Damage); }
	const FGameplayTag& GetSetByCallerStatUpOperationTag() const { return ResolveTag(SetByCallerStatUpOperationTag, LabGameplayTags::Data_StatUp); }

	const FGameplayTag& GetEquipmentEquipAbilityTag() const { return ResolveTag(EquipmentEquipAbilityTag, LabGameplayTags::Action_Equip); }
	const FGameplayTag& GetEquipmentUnequipAbilityTag() const { return ResolveTag(EquipmentUnequipAbilityTag, LabGameplayTags::Action_Unequip); }

	void GetItemEquipmentSlotTags(TArray<FGameplayTag>& OutTags) const;
	void GetItemFilterTypeTags(TArray<FGameplayTag>& OutTags) const;
	void GetPandoraFilterTypeTags(TArray<FGameplayTag>& OutTags) const;
	void GetSkinEquipmentSlotTags(TArray<FGameplayTag>& OutTags) const;
	void GetSkinFilterTypeTags(TArray<FGameplayTag>& OutTags) const;

private:
	static const FGameplayTag& ResolveTag(const FGameplayTag& ConfiguredTag, const FGameplayTag& DefaultTag);
	static void AddValidTag(TArray<FGameplayTag>& OutTags, const FGameplayTag& Tag);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Item", meta = (Categories = "Item", AllowPrivateAccess = "true"))
	FGameplayTag ItemWeaponTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Item", meta = (Categories = "Item", AllowPrivateAccess = "true"))
	FGameplayTag ItemEquipmentTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Item", meta = (Categories = "Item", AllowPrivateAccess = "true"))
	FGameplayTag ItemConsumableTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Item", meta = (Categories = "Item", AllowPrivateAccess = "true"))
	FGameplayTag ItemValuableTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Item|Equipment", meta = (Categories = "Item.Equipment", AllowPrivateAccess = "true"))
	FGameplayTag ItemHatEquipTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Item|Equipment", meta = (Categories = "Item.Equipment", AllowPrivateAccess = "true"))
	FGameplayTag ItemTopEquipTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Item|Equipment", meta = (Categories = "Item.Equipment", AllowPrivateAccess = "true"))
	FGameplayTag ItemBottomEquipTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Item|Equipment", meta = (Categories = "Item.Equipment", AllowPrivateAccess = "true"))
	FGameplayTag ItemShoesEquipTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Item|Equipment", meta = (Categories = "Item.Equipment", AllowPrivateAccess = "true"))
	FGameplayTag ItemEarringEquipTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Item|Equipment", meta = (Categories = "Item.Equipment", AllowPrivateAccess = "true"))
	FGameplayTag ItemNecklaceEquipTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Item|Equipment", meta = (Categories = "Item.Equipment", AllowPrivateAccess = "true"))
	FGameplayTag ItemRingEquipTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Item|Equipment", meta = (Categories = "Item.Equipment", AllowPrivateAccess = "true"))
	FGameplayTag ItemRuneEquipTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Pandora", meta = (Categories = "Pandora", AllowPrivateAccess = "true"))
	FGameplayTag PandoraOffensiveTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Pandora", meta = (Categories = "Pandora", AllowPrivateAccess = "true"))
	FGameplayTag PandoraDefensiveTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Pandora", meta = (Categories = "Pandora", AllowPrivateAccess = "true"))
	FGameplayTag PandoraSupportTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Pandora", meta = (Categories = "Pandora", AllowPrivateAccess = "true"))
	FGameplayTag PandoraSpecialTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Skin", meta = (Categories = "Skin", AllowPrivateAccess = "true"))
	FGameplayTag SkinPandoraTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Skin", meta = (Categories = "Skin", AllowPrivateAccess = "true"))
	FGameplayTag SkinCosmeticsTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Skin", meta = (Categories = "Skin", AllowPrivateAccess = "true"))
	FGameplayTag SkinGestureTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Skin", meta = (Categories = "Skin", AllowPrivateAccess = "true"))
	FGameplayTag SkinRidingTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Skin", meta = (Categories = "Skin", AllowPrivateAccess = "true"))
	FGameplayTag SkinPetTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Skin|Equipment", meta = (Categories = "Skin.Cosmetics", AllowPrivateAccess = "true"))
	FGameplayTag SkinHatEquipTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Skin|Equipment", meta = (Categories = "Skin.Cosmetics", AllowPrivateAccess = "true"))
	FGameplayTag SkinTopEquipTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Skin|Equipment", meta = (Categories = "Skin.Cosmetics", AllowPrivateAccess = "true"))
	FGameplayTag SkinBottomEquipTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Skin|Equipment", meta = (Categories = "Skin.Cosmetics", AllowPrivateAccess = "true"))
	FGameplayTag SkinShoesEquipTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Skin|Equipment", meta = (Categories = "Skin.Cosmetics", AllowPrivateAccess = "true"))
	FGameplayTag SkinHeadEquipTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Skin|Equipment", meta = (Categories = "Skin.Cosmetics", AllowPrivateAccess = "true"))
	FGameplayTag SkinColorEquipTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Skin|Equipment", meta = (Categories = "Skin.Cosmetics", AllowPrivateAccess = "true"))
	FGameplayTag SkinBackEquipTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Skin|Equipment", meta = (Categories = "Skin.Cosmetics", AllowPrivateAccess = "true"))
	FGameplayTag SkinAuraEquipTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|UI", meta = (Categories = "UI", AllowPrivateAccess = "true"))
	FGameplayTag UiProfileLeftTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|UI", meta = (Categories = "UI", AllowPrivateAccess = "true"))
	FGameplayTag UiStatusRightTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|UI", meta = (Categories = "UI", AllowPrivateAccess = "true"))
	FGameplayTag UiEquipmentLeftTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|UI", meta = (Categories = "UI", AllowPrivateAccess = "true"))
	FGameplayTag UiInventoryRightTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|UI", meta = (Categories = "UI", AllowPrivateAccess = "true"))
	FGameplayTag UiSkinEquipmentLeftTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|UI", meta = (Categories = "UI", AllowPrivateAccess = "true"))
	FGameplayTag UiSkinInventoryRightTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|UI", meta = (Categories = "UI", AllowPrivateAccess = "true"))
	FGameplayTag UiPandoraEquipmentLeftTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|UI", meta = (Categories = "UI", AllowPrivateAccess = "true"))
	FGameplayTag UiPandoraInventoryRightTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Status", meta = (Categories = "Status.Offense", AllowPrivateAccess = "true"))
	FGameplayTag StatusStrengthTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Status", meta = (Categories = "Status.Offense", AllowPrivateAccess = "true"))
	FGameplayTag StatusIntelligenceTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Status", meta = (Categories = "Status.Agility", AllowPrivateAccess = "true"))
	FGameplayTag StatusArcaneTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Status", meta = (Categories = "Status.Defense", AllowPrivateAccess = "true"))
	FGameplayTag StatusArmorTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Status", meta = (Categories = "Status.Defense", AllowPrivateAccess = "true"))
	FGameplayTag StatusRecoveryTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Status", meta = (Categories = "Status.Resistance", AllowPrivateAccess = "true", DisplayName = "Status Frostbite Tag"))
	FGameplayTag StatusFrostbiteTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Status", meta = (Categories = "Status.Resistance", AllowPrivateAccess = "true", DisplayName = "Status Burn Tag"))
	FGameplayTag StatusBurnTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Status", meta = (Categories = "Status.Resistance", AllowPrivateAccess = "true", DisplayName = "Status Electric Shock Tag"))
	FGameplayTag StatusElectricShockTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Status", meta = (Categories = "Status.PandoraForce", AllowPrivateAccess = "true"))
	FGameplayTag StatusFirstPandoraTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Status", meta = (Categories = "Status.PandoraForce", AllowPrivateAccess = "true"))
	FGameplayTag StatusSecondPandoraTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Status", meta = (Categories = "Status.PandoraForce", AllowPrivateAccess = "true"))
	FGameplayTag StatusThirdPandoraTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Status", meta = (Categories = "Status.Resource", AllowPrivateAccess = "true"))
	FGameplayTag StatusMaxHealthTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Status", meta = (Categories = "Status.Defense", AllowPrivateAccess = "true"))
	FGameplayTag StatusMaxShieldTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Status", meta = (Categories = "Status.Resource", AllowPrivateAccess = "true"))
	FGameplayTag StatusMaxManaTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Status", meta = (Categories = "Status.Resource", AllowPrivateAccess = "true"))
	FGameplayTag StatusMaxStaminaTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Status", meta = (Categories = "Status.Agility", AllowPrivateAccess = "true"))
	FGameplayTag StatusAttackSpeedTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Status", meta = (Categories = "Status.Agility", AllowPrivateAccess = "true"))
	FGameplayTag StatusMovementSpeedTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Status", meta = (Categories = "Status.Offense", AllowPrivateAccess = "true"))
	FGameplayTag StatusCriticalTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Combat|Ability", meta = (Categories = "Action", AllowPrivateAccess = "true"))
	FGameplayTag CombatAttackAbilityTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Combat|Ability", meta = (Categories = "Action", AllowPrivateAccess = "true"))
	FGameplayTag CombatPunchAbilityTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Combat|Ability", meta = (Categories = "Action", AllowPrivateAccess = "true"))
	FGameplayTag CombatRangedAttackAbilityTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Combat|Damage", meta = (Categories = "Status", AllowPrivateAccess = "true"))
	FGameplayTag CombatWeaponDamageSourceTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Combat|Ability", meta = (Categories = "Action", AllowPrivateAccess = "true"))
	FGameplayTag CombatHitReactAbilityTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|AbilitySystem|SetByCaller", meta = (Categories = "Data", AllowPrivateAccess = "true"))
	FGameplayTag SetByCallerDamageMagnitudeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|AbilitySystem|SetByCaller", meta = (Categories = "Data", AllowPrivateAccess = "true"))
	FGameplayTag SetByCallerStatUpOperationTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Equipment|Ability", meta = (Categories = "Action", AllowPrivateAccess = "true"))
	FGameplayTag EquipmentEquipAbilityTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tags|Equipment|Ability", meta = (Categories = "Action", AllowPrivateAccess = "true"))
	FGameplayTag EquipmentUnequipAbilityTag;
};
