#include "Definition/Common/ProjectTagDefinition.h"

#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Logging/LogRateLimiter.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProjectTagDefinition)

DEFINE_LOG_CATEGORY_STATIC(LogProjectTagDefinition, Log, All);

namespace
{
	constexpr double RequiredDefinitionLogIntervalSeconds = 30.0;
	FLogRateLimiter MissingDefinitionLogLimiter;
	FLogRateLimiter LoadFailedDefinitionLogLimiter;
	TSharedPtr<FStreamableHandle> PendingDefinitionLoadHandle;

	const UProjectTagDefinition* LoadProjectTagDefinitionPrimaryAsset()
	{
		UAssetManager* AssetManager = UAssetManager::GetIfInitialized();
		if (!AssetManager)
		{
			// 에디터·CDO 초기화 코드는 AssetManager보다 먼저 돌 수 있다.
			// Primary Asset을 쓸 수 있을 때까지는 GetDefaultDefinition()이 코드 기본값을 준다.
			return nullptr;
		}

		static FPrimaryAssetId ResolvedDefinitionId;
		if (!ResolvedDefinitionId.IsValid() || !AssetManager->GetPrimaryAssetPath(ResolvedDefinitionId).IsValid())
		{
			TArray<FPrimaryAssetId> DefinitionIds;
			AssetManager->GetPrimaryAssetIdList(UProjectTagDefinition::GetDefinitionPrimaryAssetType(), DefinitionIds);

			if (DefinitionIds.IsEmpty())
			{
				uint32 SuppressedCount = 0;
				if (MissingDefinitionLogLimiter.TryAcquire(RequiredDefinitionLogIntervalSeconds, SuppressedCount))
				{
					UE_LOG(LogProjectTagDefinition, Error,
						TEXT("No ProjectTagDefinition PrimaryAsset is registered. Expected %s. "
							"SuppressedSinceLast=%u"),
						*UProjectTagDefinition::GetPreferredPrimaryAssetId().ToString(),
						SuppressedCount);
				}
				return nullptr;
			}

			const FPrimaryAssetId PreferredId = UProjectTagDefinition::GetPreferredPrimaryAssetId();
			const FPrimaryAssetId* PreferredDefinition = DefinitionIds.FindByPredicate(
				[&PreferredId](const FPrimaryAssetId& DefinitionId)
				{
					return DefinitionId == PreferredId;
				});

			if (DefinitionIds.Num() > 1)
			{
				UE_LOG(LogProjectTagDefinition, Error,
					TEXT("Multiple ProjectTagDefinition PrimaryAssets are registered (%d). "
						"Keep exactly one. %s will be used when available."),
					DefinitionIds.Num(),
					*PreferredId.ToString());
			}

			if (PreferredDefinition)
			{
				ResolvedDefinitionId = *PreferredDefinition;
			}
			else if (DefinitionIds.Num() == 1)
			{
				// 설정 애셋이 하나뿐이면 이름이 바뀌어도 그 애셋을 쓴다.
				ResolvedDefinitionId = DefinitionIds[0];
			}
			else
			{
				return nullptr;
			}
		}

		if (const UProjectTagDefinition* LoadedDefinition =
			AssetManager->GetPrimaryAssetObject<UProjectTagDefinition>(ResolvedDefinitionId))
		{
			return LoadedDefinition;
		}

		if (!PendingDefinitionLoadHandle.IsValid() || PendingDefinitionLoadHandle->HasLoadCompleted())
		{
			PendingDefinitionLoadHandle = AssetManager->LoadPrimaryAsset(ResolvedDefinitionId);
		}

		const UProjectTagDefinition* LoadedDefinition =
			AssetManager->GetPrimaryAssetObject<UProjectTagDefinition>(ResolvedDefinitionId);
		if (!LoadedDefinition
			&& (!PendingDefinitionLoadHandle.IsValid() || PendingDefinitionLoadHandle->HasLoadCompleted()))
		{
			uint32 SuppressedCount = 0;
			if (LoadFailedDefinitionLogLimiter.TryAcquire(RequiredDefinitionLogIntervalSeconds, SuppressedCount))
			{
				UE_LOG(LogProjectTagDefinition, Error,
					TEXT("Failed to load ProjectTagDefinition PrimaryAsset %s at %s. "
						"SuppressedSinceLast=%u"),
					*ResolvedDefinitionId.ToString(),
					*AssetManager->GetPrimaryAssetPath(ResolvedDefinitionId).ToString(),
					SuppressedCount);
			}
		}

		return LoadedDefinition;
	}
}

UProjectTagDefinition::UProjectTagDefinition()
{
	ItemWeaponTypeTag = PdGameplayTags::Item_Weapon;
	ItemEquipmentTypeTag = PdGameplayTags::Item_Equipment;
	ItemConsumableTypeTag = PdGameplayTags::Item_Consumable;
	ItemValuableTypeTag = PdGameplayTags::Item_Valuable;
	ItemHatEquipTypeTag = PdGameplayTags::Item_Equipment_Hat;
	ItemTopEquipTypeTag = PdGameplayTags::Item_Equipment_Top;
	ItemBottomEquipTypeTag = PdGameplayTags::Item_Equipment_Bottom;
	ItemShoesEquipTypeTag = PdGameplayTags::Item_Equipment_Shoes;
	ItemEarringEquipTypeTag = PdGameplayTags::Item_Equipment_Earring;
	ItemNecklaceEquipTypeTag = PdGameplayTags::Item_Equipment_Necklace;
	ItemRingEquipTypeTag = PdGameplayTags::Item_Equipment_Ring;
	ItemRuneEquipTypeTag = PdGameplayTags::Item_Equipment_Rune;

	PandoraOffensiveTypeTag = PdGameplayTags::Pandora_Offensive;
	PandoraDefensiveTypeTag = PdGameplayTags::Pandora_Defensive;
	PandoraSupportTypeTag = PdGameplayTags::Pandora_Support;
	PandoraSpecialTypeTag = PdGameplayTags::Pandora_Special;

	SkinPandoraTypeTag = PdGameplayTags::Skin_Pandora;
	SkinCosmeticsTypeTag = PdGameplayTags::Skin_Cosmetics;
	SkinGestureTypeTag = PdGameplayTags::Skin_Gesture;
	SkinRidingTypeTag = PdGameplayTags::Skin_Riding;
	SkinPetTypeTag = PdGameplayTags::Skin_Pet;
	SkinHatEquipTypeTag = PdGameplayTags::Skin_Cosmetics_Hat;
	SkinTopEquipTypeTag = PdGameplayTags::Skin_Cosmetics_Top;
	SkinBottomEquipTypeTag = PdGameplayTags::Skin_Cosmetics_Bottom;
	SkinShoesEquipTypeTag = PdGameplayTags::Skin_Cosmetics_Shoes;
	SkinHeadEquipTypeTag = PdGameplayTags::Skin_Cosmetics_Head;
	SkinColorEquipTypeTag = PdGameplayTags::Skin_Cosmetics_SkinColor;
	SkinBackEquipTypeTag = PdGameplayTags::Skin_Cosmetics_Back;
	SkinAuraEquipTypeTag = PdGameplayTags::Skin_Cosmetics_Aura;

	UiProfileLeftTag = PdGameplayTags::UI_Profile;
	UiStatusRightTag = PdGameplayTags::UI_Status;
	UiEquipmentLeftTag = PdGameplayTags::UI_Equipment;
	UiInventoryRightTag = PdGameplayTags::UI_Inventory;
	UiSkinEquipmentLeftTag = PdGameplayTags::UI_SkinEquipment;
	UiSkinInventoryRightTag = PdGameplayTags::UI_SkinInventory;
	UiPandoraEquipmentLeftTag = PdGameplayTags::UI_PandoraEquipment;
	UiPandoraInventoryRightTag = PdGameplayTags::UI_PandoraInventory;

	StatusStrengthTag = PdGameplayTags::Status_Offense_Strength;
	StatusIntelligenceTag = PdGameplayTags::Status_Offense_Intelligence;
	StatusArcaneTag = PdGameplayTags::Status_Agility_Arcane;
	StatusArmorTag = PdGameplayTags::Status_Defense_Armor;
	StatusRecoveryTag = PdGameplayTags::Status_Defense_Recovery;
	StatusFrostbiteTag = PdGameplayTags::Status_Resistance_Frostbite;
	StatusBurnTag = PdGameplayTags::Status_Resistance_Burn;
	StatusElectricShockTag = PdGameplayTags::Status_Resistance_ElectricShock;
	StatusFirstPandoraTag = PdGameplayTags::Status_PandoraForce_FirstPandora;
	StatusSecondPandoraTag = PdGameplayTags::Status_PandoraForce_SecondPandora;
	StatusThirdPandoraTag = PdGameplayTags::Status_PandoraForce_ThirdPandora;
	StatusMaxHealthTag = PdGameplayTags::Status_Resource_MaxHealth;
	StatusMaxShieldTag = PdGameplayTags::Status_Defense_MaxShield;
	StatusMaxManaTag = PdGameplayTags::Status_Resource_MaxMana;
	StatusMaxStaminaTag = PdGameplayTags::Status_Resource_MaxStamina;
	StatusAttackSpeedTag = PdGameplayTags::Status_Agility_AttackSpeed;
	StatusMovementSpeedTag = PdGameplayTags::Status_Agility_MovementSpeed;
	StatusCriticalTag = PdGameplayTags::Status_Offense_Critical;

	CombatAttackAbilityTag = PdGameplayTags::Action_Attack;
	CombatPunchAbilityTag = PdGameplayTags::Action_Punch;
	CombatRangedAttackAbilityTag = PdGameplayTags::Action_RangedAttack;
	CombatWeaponDamageSourceTag = PdGameplayTags::Status_Offense_Strength;
	CombatHitReactAbilityTag = PdGameplayTags::Action_HitReact;

	SetByCallerDamageMagnitudeTag = PdGameplayTags::Data_Damage;
	SetByCallerStatUpOperationTag = PdGameplayTags::Data_StatUp;

	EquipmentEquipAbilityTag = PdGameplayTags::Action_Equip;
	EquipmentUnequipAbilityTag = PdGameplayTags::Action_Unequip;
}

FPrimaryAssetId UProjectTagDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(GetDefinitionPrimaryAssetType(), GetFName());
}

const UProjectTagDefinition* UProjectTagDefinition::Get(const UObject*)
{
	return GetDefaultDefinition();
}

const UProjectTagDefinition* UProjectTagDefinition::GetDefaultDefinition()
{
	if (const UProjectTagDefinition* PrimaryDefinition = LoadProjectTagDefinitionPrimaryAsset())
	{
		return PrimaryDefinition;
	}

	return GetDefault<UProjectTagDefinition>();
}

const FPrimaryAssetType& UProjectTagDefinition::GetDefinitionPrimaryAssetType()
{
	static const FPrimaryAssetType AssetType(TEXT("ProjectTagDefinition"));
	return AssetType;
}

const FPrimaryAssetId& UProjectTagDefinition::GetPreferredPrimaryAssetId()
{
	static const FPrimaryAssetId AssetId(GetDefinitionPrimaryAssetType(), TEXT("DA_ProjectTag"));
	return AssetId;
}

#if WITH_EDITOR
EDataValidationResult UProjectTagDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (Result == EDataValidationResult::NotValidated)
	{
		Result = EDataValidationResult::Valid;
	}

	if (const UAssetManager* AssetManager = UAssetManager::GetIfInitialized())
	{
		TArray<FPrimaryAssetId> DefinitionIds;
		AssetManager->GetPrimaryAssetIdList(GetDefinitionPrimaryAssetType(), DefinitionIds);
		if (DefinitionIds.Num() != 1)
		{
			Context.AddError(FText::Format(NSLOCTEXT("ProjectTagDefinition", "SinglePrimaryAssetRequired",
				"Exactly one ProjectTagDefinition PrimaryAsset must be registered, but {0} were found."),
				FText::AsNumber(DefinitionIds.Num())));
			Result = EDataValidationResult::Invalid;
		}
	}

	return Result;
}
#endif

void UProjectTagDefinition::GetItemEquipmentSlotTags(TArray<FGameplayTag>& OutTags) const
{
	OutTags.Reset();
	AddValidTag(OutTags, GetItemHatEquipTypeTag());
	AddValidTag(OutTags, GetItemTopEquipTypeTag());
	AddValidTag(OutTags, GetItemBottomEquipTypeTag());
	AddValidTag(OutTags, GetItemShoesEquipTypeTag());
	AddValidTag(OutTags, GetItemEarringEquipTypeTag());
	AddValidTag(OutTags, GetItemNecklaceEquipTypeTag());
	AddValidTag(OutTags, GetItemRingEquipTypeTag());
	AddValidTag(OutTags, GetItemRuneEquipTypeTag());
}

void UProjectTagDefinition::GetItemFilterTypeTags(TArray<FGameplayTag>& OutTags) const
{
	OutTags.Reset();
	AddValidTag(OutTags, GetItemWeaponTypeTag());
	AddValidTag(OutTags, GetItemEquipmentTypeTag());
	TArray<FGameplayTag> EquipmentSlotTags;
	GetItemEquipmentSlotTags(EquipmentSlotTags);
	for (const FGameplayTag& EquipmentSlotTag : EquipmentSlotTags)
	{
		AddValidTag(OutTags, EquipmentSlotTag);
	}
	AddValidTag(OutTags, GetItemConsumableTypeTag());
	AddValidTag(OutTags, GetItemValuableTypeTag());
}

// 분류용 상위 태그와 실제 장착 슬롯을 구분해 서버가 허용하는 슬롯만 나열한다.
void UProjectTagDefinition::GetSkinEquipmentSlotTags(TArray<FGameplayTag>& OutTags) const
{
	OutTags.Reset();
	AddValidTag(OutTags, GetSkinHatEquipTypeTag());
	AddValidTag(OutTags, GetSkinTopEquipTypeTag());
	AddValidTag(OutTags, GetSkinBottomEquipTypeTag());
	AddValidTag(OutTags, GetSkinShoesEquipTypeTag());
	AddValidTag(OutTags, GetSkinHeadEquipTypeTag());
	AddValidTag(OutTags, GetSkinColorEquipTypeTag());
	AddValidTag(OutTags, GetSkinBackEquipTypeTag());
	AddValidTag(OutTags, GetSkinAuraEquipTypeTag());
	AddValidTag(OutTags, GetSkinRidingTypeTag());
	AddValidTag(OutTags, GetSkinPetTypeTag());
	AddValidTag(OutTags, PdGameplayTags::Skin_Gesture_Slot1);
	AddValidTag(OutTags, PdGameplayTags::Skin_Gesture_Slot2);
	AddValidTag(OutTags, PdGameplayTags::Skin_Gesture_Slot3);
	AddValidTag(OutTags, PdGameplayTags::Skin_Gesture_Slot4);
}

void UProjectTagDefinition::GetSkinFilterTypeTags(TArray<FGameplayTag>& OutTags) const
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

const FGameplayTag& UProjectTagDefinition::ResolveTag(const FGameplayTag& ConfiguredTag, const FGameplayTag& DefaultTag)
{
	return ConfiguredTag.IsValid() ? ConfiguredTag : DefaultTag;
}

void UProjectTagDefinition::AddValidTag(TArray<FGameplayTag>& OutTags, const FGameplayTag& Tag)
{
	if (Tag.IsValid())
	{
		OutTags.AddUnique(Tag);
	}
}
