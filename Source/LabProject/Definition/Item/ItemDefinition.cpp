#include "Definition/Item/ItemDefinition.h"

#include "Common/LabGameplayTags.h"
#include "Definition/Common/ProjectTagConfig.h"
#include "GameplayEffect.h"

#if WITH_EDITOR
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Materials/MaterialInterface.h"
#include "Misc/DataValidation.h"
#include "Weapon/WeaponBase.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(ItemDefinition)

namespace
{
#if WITH_EDITOR
	void MarkItemInvalid(FDataValidationContext& Context, EDataValidationResult& Result, const FText& Message)
	{
		Result = EDataValidationResult::Invalid;
		Context.AddError(Message);
	}

	FText ItemFieldText(const TCHAR* FieldName)
	{
		return FText::FromString(FString(FieldName));
	}

	template <typename ObjectType>
	void ValidateSoftObjectReference(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const TSoftObjectPtr<ObjectType>& SoftObject,
		const TCHAR* FieldName)
	{
		if (!SoftObject.IsNull() && !SoftObject.LoadSynchronous())
		{
			MarkItemInvalid(Context, Result, FText::Format(
				NSLOCTEXT("ItemDefinition", "InvalidSoftObjectReference", "{0} could not be loaded: {1}"),
				ItemFieldText(FieldName),
				FText::FromString(SoftObject.ToString())));
		}
	}

	template <typename ClassType>
	void ValidateSoftClassReference(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const TSoftClassPtr<ClassType>& SoftClass,
		const TCHAR* FieldName)
	{
		if (!SoftClass.IsNull() && !SoftClass.LoadSynchronous())
		{
			MarkItemInvalid(Context, Result, FText::Format(
				NSLOCTEXT("ItemDefinition", "InvalidSoftClassReference", "{0} could not be loaded: {1}"),
				ItemFieldText(FieldName),
				FText::FromString(SoftClass.ToString())));
		}
	}

	void ValidateMagnitudeMap(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const TMap<FGameplayTag, float>& Magnitudes,
		const TCHAR* FieldName)
	{
		for (const TPair<FGameplayTag, float>& Pair : Magnitudes)
		{
			if (!Pair.Key.IsValid())
			{
				MarkItemInvalid(Context, Result, FText::Format(
					NSLOCTEXT("ItemDefinition", "InvalidMagnitudeTag", "{0} contains an invalid GameplayTag."),
					ItemFieldText(FieldName)));
			}

			if (!FMath::IsFinite(Pair.Value))
			{
				MarkItemInvalid(Context, Result, FText::Format(
					NSLOCTEXT("ItemDefinition", "InvalidMagnitudeValue", "{0} contains a non-finite magnitude for tag {1}."),
					ItemFieldText(FieldName),
					FText::FromString(Pair.Key.ToString())));
			}
			else if (FMath::IsNearlyZero(Pair.Value))
			{
				Context.AddWarning(FText::Format(
					NSLOCTEXT("ItemDefinition", "ZeroMagnitudeValue", "{0} contains a zero magnitude for tag {1}. Runtime code will ignore it."),
					ItemFieldText(FieldName),
					FText::FromString(Pair.Key.ToString())));
			}
		}
	}

	bool MatchesAnyKnownItemType(const UItemDefinition& ItemDefinition)
	{
		const UProjectTagConfig* ProjectTagConfig = UProjectTagConfig::GetDefaultConfig();
		if (!ProjectTagConfig)
		{
			return false;
		}

		TArray<FGameplayTag> ItemTypeTags;
		ProjectTagConfig->GetItemFilterTypeTags(ItemTypeTags);
		for (const FGameplayTag& ItemTypeTag : ItemTypeTags)
		{
			if (ItemDefinition.MatchesItemType(ItemTypeTag))
			{
				return true;
			}
		}

		return false;
	}

	void ValidateItemIdentity(FDataValidationContext& Context, EDataValidationResult& Result, const UItemDefinition& ItemDefinition)
	{
		if (ItemDefinition.DisplayName.IsEmpty())
		{
			Context.AddWarning(NSLOCTEXT("ItemDefinition", "MissingDisplayName", "DisplayName is empty. UI will fall back to the asset name in some places."));
		}

		if (ItemDefinition.IconTexture.IsNull())
		{
			Context.AddWarning(NSLOCTEXT("ItemDefinition", "MissingIconTexture", "IconTexture is not set."));
		}
		else
		{
			ValidateSoftObjectReference(Context, Result, ItemDefinition.IconTexture, TEXT("IconTexture"));
		}

		if (!ItemDefinition.IdTag.IsValid())
		{
			MarkItemInvalid(Context, Result, NSLOCTEXT("ItemDefinition", "MissingIdTag", "IdTag is required."));
		}
		else if (!MatchesAnyKnownItemType(ItemDefinition))
		{
			Context.AddWarning(FText::Format(
				NSLOCTEXT("ItemDefinition", "UnknownItemTypeTag", "IdTag does not match a configured item type tag: {0}"),
				FText::FromString(ItemDefinition.IdTag.ToString())));
		}
	}

	void ValidateItemShopData(FDataValidationContext& Context, EDataValidationResult& Result, const FShopProductDefinitionData& ShopData)
	{
		if (ShopData.GoldPrice < 0)
		{
			MarkItemInvalid(Context, Result, NSLOCTEXT("ItemDefinition", "InvalidShopGoldPrice", "ShopData.GoldPrice cannot be negative."));
		}
	}

	void ValidateDropData(FDataValidationContext& Context, EDataValidationResult& Result, const UItemDefinition& ItemDefinition)
	{
		if (!FMath::IsFinite(ItemDefinition.DropRate))
		{
			MarkItemInvalid(Context, Result, NSLOCTEXT("ItemDefinition", "InvalidDropRateFinite", "DropRate must be finite."));
		}
		else if (ItemDefinition.DropRate < 0.0f)
		{
			MarkItemInvalid(Context, Result, NSLOCTEXT("ItemDefinition", "InvalidDropRateNegative", "DropRate cannot be negative."));
		}
		else if (ItemDefinition.bCanDropFromRewardChest && FMath::IsNearlyZero(ItemDefinition.DropRate))
		{
			Context.AddWarning(NSLOCTEXT("ItemDefinition", "ZeroRewardChestDropRate", "bCanDropFromRewardChest is true, but DropRate is zero. Reward chests will not select this item."));
		}
	}

	void ValidateConsumableData(FDataValidationContext& Context, EDataValidationResult& Result, const UItemDefinition& ItemDefinition)
	{
		const UProjectTagConfig* ProjectTagConfig = UProjectTagConfig::GetDefaultConfig();
		const FGameplayTag ConsumableTypeTag = ProjectTagConfig
			? ProjectTagConfig->GetItemConsumableTypeTag()
			: LabGameplayTags::Item_Consumable;
		const bool bConsumableByTag = ItemDefinition.IsConsumableDefinition(ConsumableTypeTag);
		const bool bHasConsumableEffect = ItemDefinition.ConsumeGameplayEffectClass != nullptr;

		if (ItemDefinition.QuantityToConsume < 1)
		{
			MarkItemInvalid(Context, Result, NSLOCTEXT("ItemDefinition", "InvalidQuantityToConsume", "QuantityToConsume must be at least 1."));
		}

		if (bConsumableByTag && !bHasConsumableEffect)
		{
			MarkItemInvalid(Context, Result, NSLOCTEXT("ItemDefinition", "MissingConsumableEffect", "Consumable items require ConsumeGameplayEffectClass."));
		}

		if (!bHasConsumableEffect && !ItemDefinition.Map_Consume_Magnitude.IsEmpty())
		{
			MarkItemInvalid(Context, Result, NSLOCTEXT("ItemDefinition", "ConsumeMagnitudesWithoutEffect", "Map_Consume_Magnitude is set, but ConsumeGameplayEffectClass is missing."));
		}

		if (bHasConsumableEffect && !bConsumableByTag)
		{
			Context.AddWarning(NSLOCTEXT("ItemDefinition", "ConsumableEffectWithoutTag", "ConsumeGameplayEffectClass is set, but IdTag does not match the consumable item type. Quick slot usage will not treat this as a consumable."));
		}

		ValidateMagnitudeMap(Context, Result, ItemDefinition.Map_Consume_Magnitude, TEXT("Map_Consume_Magnitude"));
	}

	void ValidateItemAimCameraSettings(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const FWeaponAimCameraSettings& CameraSettings,
		const TCHAR* FieldPrefix)
	{
		if (!FMath::IsFinite(CameraSettings.TargetFOV) || CameraSettings.TargetFOV <= 0.0f)
		{
			MarkItemInvalid(Context, Result, FText::Format(
				NSLOCTEXT("ItemDefinition", "InvalidAimCameraFov", "{0}.TargetFOV must be a positive finite value."),
				ItemFieldText(FieldPrefix)));
		}
		else if (CameraSettings.TargetFOV < 5.0f || CameraSettings.TargetFOV > 170.0f)
		{
			Context.AddWarning(FText::Format(
				NSLOCTEXT("ItemDefinition", "AimCameraFovOutOfRange", "{0}.TargetFOV will be clamped to 5..170 by camera code."),
				ItemFieldText(FieldPrefix)));
		}

		if (CameraSettings.TargetBoomSocketOffset.ContainsNaN())
		{
			MarkItemInvalid(Context, Result, FText::Format(
				NSLOCTEXT("ItemDefinition", "InvalidAimCameraOffset", "{0}.TargetBoomSocketOffset contains NaN."),
				ItemFieldText(FieldPrefix)));
		}

		if (CameraSettings.TargetCameraRotation.ContainsNaN())
		{
			MarkItemInvalid(Context, Result, FText::Format(
				NSLOCTEXT("ItemDefinition", "InvalidAimCameraRotation", "{0}.TargetCameraRotation contains NaN."),
				ItemFieldText(FieldPrefix)));
		}

		if (!FMath::IsFinite(CameraSettings.InterpSpeed) || CameraSettings.InterpSpeed < 0.0f)
		{
			MarkItemInvalid(Context, Result, FText::Format(
				NSLOCTEXT("ItemDefinition", "InvalidAimCameraInterpSpeed", "{0}.InterpSpeed must be a non-negative finite value."),
				ItemFieldText(FieldPrefix)));
		}
	}

	void ValidateWeaponData(FDataValidationContext& Context, EDataValidationResult& Result, const UItemDefinition& ItemDefinition)
	{
		const UProjectTagConfig* ProjectTagConfig = UProjectTagConfig::GetDefaultConfig();
		const FGameplayTag WeaponTypeTag = ProjectTagConfig
			? ProjectTagConfig->GetItemWeaponTypeTag()
			: LabGameplayTags::Item_Weapon;
		const bool bWeaponByTag = ItemDefinition.MatchesItemType(WeaponTypeTag);
		const bool bHasWeaponData = ItemDefinition.HasWeaponData();

		if (!bWeaponByTag && !bHasWeaponData)
		{
			return;
		}

		if (bHasWeaponData && !bWeaponByTag)
		{
			Context.AddWarning(NSLOCTEXT("ItemDefinition", "WeaponDataWithoutWeaponTag", "WeaponData is set, but IdTag does not match the weapon item type. Inventory filtering may not show it as a weapon."));
		}

		ValidateSoftClassReference(Context, Result, ItemDefinition.WeaponData.Equip.ActorClass, TEXT("WeaponData.Equip.ActorClass"));
		ValidateSoftObjectReference(Context, Result, ItemDefinition.WeaponData.Equip.EquipMontage, TEXT("WeaponData.Equip.EquipMontage"));
		ValidateSoftObjectReference(Context, Result, ItemDefinition.WeaponData.Equip.UnequipMontage, TEXT("WeaponData.Equip.UnequipMontage"));
		ValidateSoftClassReference(Context, Result, ItemDefinition.WeaponData.Equip.AnimLayer, TEXT("WeaponData.Equip.AnimLayer"));
		ValidateSoftObjectReference(Context, Result, ItemDefinition.WeaponData.Attack.AttackMontage, TEXT("WeaponData.Attack.AttackMontage"));
		ValidateSoftObjectReference(Context, Result, ItemDefinition.WeaponData.HitReact.HitReactMontage, TEXT("WeaponData.HitReact.HitReactMontage"));

		if (!FMath::IsFinite(ItemDefinition.WeaponData.Attack.StaminaCost)
			|| ItemDefinition.WeaponData.Attack.StaminaCost < 0.0f)
		{
			MarkItemInvalid(Context, Result, NSLOCTEXT("ItemDefinition", "InvalidWeaponAttackStaminaCost", "WeaponData.Attack.StaminaCost must be a non-negative finite value."));
		}

		if (!FMath::IsFinite(ItemDefinition.WeaponData.Movement.MeleeEquippedMovementSpeedMultiplier)
			|| ItemDefinition.WeaponData.Movement.MeleeEquippedMovementSpeedMultiplier <= 0.0f)
		{
			MarkItemInvalid(Context, Result, NSLOCTEXT("ItemDefinition", "InvalidMeleeMovementSpeedMultiplier", "WeaponData.Movement.MeleeEquippedMovementSpeedMultiplier must be a positive finite value."));
		}

		if (ItemDefinition.WeaponData.Equip.ActorClass.IsNull())
		{
			MarkItemInvalid(Context, Result, NSLOCTEXT("ItemDefinition", "WeaponMissingActorClass", "Weapon items require WeaponData.Equip.ActorClass."));
		}

		if (ItemDefinition.WeaponData.Equip.AttachSocketName == EWeaponSocketName::None)
		{
			Context.AddWarning(NSLOCTEXT("ItemDefinition", "WeaponMissingAttachSocket", "WeaponData.Equip.AttachSocketName is None. The weapon may attach to the wrong place."));
		}

		if (ItemDefinition.WeaponData.Aim.CrosshairWidgetTag.IsValid() && !ItemDefinition.WeaponData.Aim.bSupportsInput)
		{
			Context.AddWarning(NSLOCTEXT("ItemDefinition", "AimCrosshairWithoutInput", "WeaponData.Aim.CrosshairWidgetTag is set, but bSupportsInput is false."));
		}

		if (ItemDefinition.WeaponData.Aim.bSupportsInput && !ItemDefinition.WeaponData.Aim.CrosshairWidgetTag.IsValid())
		{
			Context.AddWarning(NSLOCTEXT("ItemDefinition", "AimInputWithoutCrosshair", "WeaponData.Aim.bSupportsInput is true, but CrosshairWidgetTag is not set."));
		}

		if (!FMath::IsFinite(ItemDefinition.WeaponData.Aim.MaxAcceptedServerViewDistance)
			|| ItemDefinition.WeaponData.Aim.MaxAcceptedServerViewDistance < 0.0f)
		{
			MarkItemInvalid(Context, Result, NSLOCTEXT("ItemDefinition", "InvalidAimServerViewDistance", "WeaponData.Aim.MaxAcceptedServerViewDistance must be a non-negative finite value."));
		}

		ValidateItemAimCameraSettings(Context, Result, ItemDefinition.WeaponData.Aim.CameraSettings, TEXT("WeaponData.Aim.CameraSettings"));

		if (!FMath::IsFinite(ItemDefinition.WeaponData.Bow.TraceRange) || ItemDefinition.WeaponData.Bow.TraceRange <= 0.0f)
		{
			MarkItemInvalid(Context, Result, NSLOCTEXT("ItemDefinition", "InvalidBowTraceRange", "WeaponData.Bow.TraceRange must be a positive finite value."));
		}

		if (!FMath::IsFinite(ItemDefinition.WeaponData.Bow.MaxAcceptedServerLaunchStartDistance)
			|| ItemDefinition.WeaponData.Bow.MaxAcceptedServerLaunchStartDistance < 0.0f)
		{
			MarkItemInvalid(Context, Result, NSLOCTEXT("ItemDefinition", "InvalidBowServerLaunchDistance", "WeaponData.Bow.MaxAcceptedServerLaunchStartDistance must be a non-negative finite value."));
		}

		if (!FMath::IsFinite(ItemDefinition.WeaponData.Bow.MinimumDrawDuration)
			|| ItemDefinition.WeaponData.Bow.MinimumDrawDuration <= 0.0f)
		{
			MarkItemInvalid(Context, Result, NSLOCTEXT("ItemDefinition", "InvalidBowMinimumDrawDuration", "WeaponData.Bow.MinimumDrawDuration must be a positive finite value."));
		}

		if (!FMath::IsFinite(ItemDefinition.WeaponData.Bow.FireInterval)
			|| ItemDefinition.WeaponData.Bow.FireInterval <= 0.0f)
		{
			MarkItemInvalid(Context, Result, NSLOCTEXT("ItemDefinition", "InvalidBowFireInterval", "WeaponData.Bow.FireInterval must be a positive finite value."));
		}

		ValidateSoftObjectReference(Context, Result, ItemDefinition.WeaponData.Bow.WeaponMontage, TEXT("WeaponData.Bow.WeaponMontage"));

		if (ItemDefinition.WeaponData.Bow.HasAnyData())
		{
			if (!ItemDefinition.WeaponData.Bow.ArrowActorClass)
			{
				MarkItemInvalid(Context, Result, NSLOCTEXT("ItemDefinition", "BowMissingArrowClass", "Bow weapon data requires WeaponData.Bow.ArrowActorClass."));
			}

			if (ItemDefinition.WeaponData.Bow.TraceObjectTypes.IsEmpty())
			{
				Context.AddWarning(NSLOCTEXT("ItemDefinition", "BowEmptyTraceObjectTypes", "WeaponData.Bow.TraceObjectTypes is empty. Bow aim and launch traces may not hit anything."));
			}
		}

		if (!FMath::IsFinite(ItemDefinition.WeaponData.Gun.TraceRange) || ItemDefinition.WeaponData.Gun.TraceRange <= 0.0f)
		{
			MarkItemInvalid(Context, Result, NSLOCTEXT("ItemDefinition", "InvalidGunTraceRange", "WeaponData.Gun.TraceRange must be a positive finite value."));
		}

		if (!FMath::IsFinite(ItemDefinition.WeaponData.Gun.TraceRadius) || ItemDefinition.WeaponData.Gun.TraceRadius < 0.0f)
		{
			MarkItemInvalid(Context, Result, NSLOCTEXT("ItemDefinition", "InvalidGunTraceRadius", "WeaponData.Gun.TraceRadius must be a non-negative finite value."));
		}

		if (!FMath::IsFinite(ItemDefinition.WeaponData.Gun.FireInterval) || ItemDefinition.WeaponData.Gun.FireInterval <= 0.0f)
		{
			MarkItemInvalid(Context, Result, NSLOCTEXT("ItemDefinition", "InvalidGunFireInterval", "WeaponData.Gun.FireInterval must be positive."));
		}

		const FVector2D& ImpactDecalSizeRange = ItemDefinition.WeaponData.Gun.ImpactDecalSizeRange;
		if (!FMath::IsFinite(ImpactDecalSizeRange.X)
			|| !FMath::IsFinite(ImpactDecalSizeRange.Y)
			|| ImpactDecalSizeRange.X < 0.0f
			|| ImpactDecalSizeRange.Y < 0.0f)
		{
			MarkItemInvalid(Context, Result, NSLOCTEXT("ItemDefinition", "InvalidGunImpactDecalSizeRange", "WeaponData.Gun.ImpactDecalSizeRange must contain non-negative finite values."));
		}
		else if (ImpactDecalSizeRange.X > ImpactDecalSizeRange.Y)
		{
			MarkItemInvalid(Context, Result, NSLOCTEXT("ItemDefinition", "InvertedGunImpactDecalSizeRange", "WeaponData.Gun.ImpactDecalSizeRange minimum cannot be greater than maximum."));
		}

		if (!FMath::IsFinite(ItemDefinition.WeaponData.Gun.ImpactDecalDepth)
			|| ItemDefinition.WeaponData.Gun.ImpactDecalDepth < 0.0f)
		{
			MarkItemInvalid(Context, Result, NSLOCTEXT("ItemDefinition", "InvalidGunImpactDecalDepth", "WeaponData.Gun.ImpactDecalDepth must be a non-negative finite value."));
		}

		if (!FMath::IsFinite(ItemDefinition.WeaponData.Gun.ImpactDecalLifeSpan)
			|| ItemDefinition.WeaponData.Gun.ImpactDecalLifeSpan < 0.0f)
		{
			MarkItemInvalid(Context, Result, NSLOCTEXT("ItemDefinition", "InvalidGunImpactDecalLifeSpan", "WeaponData.Gun.ImpactDecalLifeSpan must be a non-negative finite value."));
		}

		if (ItemDefinition.WeaponData.Gun.ImpactDecalRotationOffset.ContainsNaN())
		{
			MarkItemInvalid(Context, Result, NSLOCTEXT("ItemDefinition", "InvalidGunImpactDecalRotation", "WeaponData.Gun.ImpactDecalRotationOffset contains NaN."));
		}

		ValidateSoftObjectReference(Context, Result, ItemDefinition.WeaponData.Gun.ImpactDecalMaterial, TEXT("WeaponData.Gun.ImpactDecalMaterial"));

		if (ItemDefinition.WeaponData.Gun.HasAnyData() && ItemDefinition.WeaponData.Gun.TraceObjectTypes.IsEmpty())
		{
			Context.AddWarning(NSLOCTEXT("ItemDefinition", "GunEmptyTraceObjectTypes", "WeaponData.Gun.TraceObjectTypes is empty. Gun traces may not hit anything."));
		}

		if (ItemDefinition.WeaponData.Bow.HasAnyData() && ItemDefinition.WeaponData.Gun.HasAnyData())
		{
			Context.AddWarning(NSLOCTEXT("ItemDefinition", "BowAndGunDataSet", "Both Bow and Gun weapon data are set. Confirm that the weapon actor is expected to use both."));
		}

		if (!FMath::IsFinite(ItemDefinition.WeaponData.AI.RangedTargetLockDelay)
			|| ItemDefinition.WeaponData.AI.RangedTargetLockDelay < 0.0f)
		{
			MarkItemInvalid(Context, Result, NSLOCTEXT("ItemDefinition", "InvalidAiRangedTargetLockDelay", "WeaponData.AI.RangedTargetLockDelay must be a non-negative finite value."));
		}
	}
#endif
}

UItemDefinition::UItemDefinition()
{
}

FPrimaryAssetId UItemDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("ItemDefinition"), GetFName());
}

FName UItemDefinition::GetWeaponPresentationBundleName()
{
	static const FName WeaponPresentationBundleName(TEXT("WeaponPresentation"));
	return WeaponPresentationBundleName;
}

void UItemDefinition::GetWeaponPresentationAssetPaths(TArray<FSoftObjectPath>& OutAssetPaths) const
{
	auto AddPath = [&OutAssetPaths](const FSoftObjectPath& AssetPath)
	{
		if (!AssetPath.IsNull())
		{
			OutAssetPaths.AddUnique(AssetPath);
		}
	};

	AddPath(WeaponData.Equip.ActorClass.ToSoftObjectPath());
	AddPath(WeaponData.Equip.EquipMontage.ToSoftObjectPath());
	AddPath(WeaponData.Equip.UnequipMontage.ToSoftObjectPath());
	AddPath(WeaponData.Equip.AnimLayer.ToSoftObjectPath());
	AddPath(WeaponData.Attack.AttackMontage.ToSoftObjectPath());
	AddPath(WeaponData.HitReact.HitReactMontage.ToSoftObjectPath());
	AddPath(WeaponData.Bow.WeaponMontage.ToSoftObjectPath());
	AddPath(WeaponData.Gun.ImpactDecalMaterial.ToSoftObjectPath());
}

#if WITH_EDITOR
EDataValidationResult UItemDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (Result == EDataValidationResult::NotValidated)
	{
		Result = EDataValidationResult::Valid;
	}

	ValidateItemIdentity(Context, Result, *this);
	ValidateMagnitudeMap(Context, Result, Map_Stat_Magnitude, TEXT("Map_Stat_Magnitude"));
	ValidateConsumableData(Context, Result, *this);
	ValidateDropData(Context, Result, *this);
	ValidateItemShopData(Context, Result, ShopData);
	ValidateWeaponData(Context, Result, *this);

	return Result;
}
#endif

bool UItemDefinition::MatchesItemType(const FGameplayTag ItemTypeTag) const
{
	return IdTag.IsValid() && ItemTypeTag.IsValid() && IdTag.MatchesTag(ItemTypeTag);
}

bool UItemDefinition::HasWeaponData() const
{
	return WeaponData.HasAnyData();
}

bool UItemDefinition::IsWeaponDefinition(const FGameplayTag WeaponTypeTag) const
{
	return MatchesItemType(WeaponTypeTag) || HasWeaponData();
}

float UItemDefinition::GetSafeAttackStaminaCost() const
{
	return FMath::IsFinite(WeaponData.Attack.StaminaCost)
		? FMath::Max(WeaponData.Attack.StaminaCost, 0.0f)
		: 0.0f;
}

float UItemDefinition::GetEquippedMovementSpeedMultiplier() const
{
	if (WeaponData.Aim.bSupportsInput)
	{
		return 1.0f;
	}

	const float ConfiguredMultiplier =
		WeaponData.Movement.MeleeEquippedMovementSpeedMultiplier;
	return FMath::IsFinite(ConfiguredMultiplier)
		? FMath::Max(ConfiguredMultiplier, 0.01f)
		: 1.0f;
}

bool UItemDefinition::IsConsumableDefinition(const FGameplayTag ConsumableTypeTag) const
{
	return MatchesItemType(ConsumableTypeTag);
}

bool UItemDefinition::CanDropFromRewardChest() const
{
	return bCanDropFromRewardChest && GetRewardChestDropWeight() > 0.0f;
}

float UItemDefinition::GetRewardChestDropWeight() const
{
	return FMath::Max(0.0f, DropRate);
}

int32 UItemDefinition::GetSafeQuantityToConsume() const
{
	return FMath::Max(1, QuantityToConsume);
}
