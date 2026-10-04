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
		if (!ResolvedDefinitionId.IsValid()
			|| !AssetManager->GetPrimaryAssetPath(ResolvedDefinitionId).IsValid())
		{
			TArray<FPrimaryAssetId> DefinitionIds;
			AssetManager->GetPrimaryAssetIdList(UProjectTagDefinition::GetDefinitionPrimaryAssetType(), DefinitionIds);

			if (DefinitionIds.IsEmpty())
			{
				uint32 SuppressedCount = 0;
				if (MissingDefinitionLogLimiter.TryAcquire(
					RequiredDefinitionLogIntervalSeconds,
					SuppressedCount))
				{
					UE_LOG(
						LogProjectTagDefinition,
						Error,
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
				UE_LOG(
					LogProjectTagDefinition,
					Error,
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

		if (!PendingDefinitionLoadHandle.IsValid()
			|| PendingDefinitionLoadHandle->HasLoadCompleted())
		{
			PendingDefinitionLoadHandle =
				AssetManager->LoadPrimaryAsset(ResolvedDefinitionId);
		}

		const UProjectTagDefinition* LoadedDefinition =
			AssetManager->GetPrimaryAssetObject<UProjectTagDefinition>(ResolvedDefinitionId);
		if (!LoadedDefinition
			&& (!PendingDefinitionLoadHandle.IsValid()
				|| PendingDefinitionLoadHandle->HasLoadCompleted()))
		{
			uint32 SuppressedCount = 0;
			if (LoadFailedDefinitionLogLimiter.TryAcquire(
				RequiredDefinitionLogIntervalSeconds,
				SuppressedCount))
			{
				UE_LOG(
					LogProjectTagDefinition,
					Error,
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
			Context.AddError(FText::Format(
				NSLOCTEXT(
					"ProjectTagDefinition",
					"SinglePrimaryAssetRequired",
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
	AddValidTag(OutTags, LabGameplayTags::Skin_Gesture_Slot1);
	AddValidTag(OutTags, LabGameplayTags::Skin_Gesture_Slot2);
	AddValidTag(OutTags, LabGameplayTags::Skin_Gesture_Slot3);
	AddValidTag(OutTags, LabGameplayTags::Skin_Gesture_Slot4);
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
