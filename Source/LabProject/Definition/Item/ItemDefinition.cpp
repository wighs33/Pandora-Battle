#include "Definition/Item/ItemDefinition.h"

#include "Common/PdGameplayTags.h"
#include "Definition/Common/ProjectTagDefinition.h"
#include "Engine/AssetManager.h"
#include "GameplayEffect.h"

#if WITH_EDITOR
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Materials/MaterialInterface.h"
#include "Misc/DataValidation.h"
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
	void ValidateSoftObjectReference(FDataValidationContext& Context, EDataValidationResult& Result,
		const TSoftObjectPtr<ObjectType>& SoftObject, const TCHAR* FieldName)
	{
		if (!SoftObject.IsNull() && !SoftObject.LoadSynchronous())
		{
			MarkItemInvalid(Context, Result, FText::Format(
				NSLOCTEXT("ItemDefinition", "InvalidSoftObjectReference", "{0} could not be loaded: {1}"),
				ItemFieldText(FieldName), FText::FromString(SoftObject.ToString())));
		}
	}

	template <typename ClassType>
	void ValidateSoftClassReference(FDataValidationContext& Context, EDataValidationResult& Result,
		const TSoftClassPtr<ClassType>& SoftClass, const TCHAR* FieldName)
	{
		// 클래스 타입을 몰라도 되도록 경로로 불러와 확인한다. 허용 타입은 편집기 속성이 이미 제한한다.
		if (!SoftClass.IsNull() && !SoftClass.ToSoftObjectPath().TryLoad())
		{
			MarkItemInvalid(Context, Result, FText::Format(
				NSLOCTEXT("ItemDefinition", "InvalidSoftClassReference", "{0} could not be loaded: {1}"),
				ItemFieldText(FieldName), FText::FromString(SoftClass.ToString())));
		}
	}

	void ValidateMagnitudeMap(FDataValidationContext& Context, EDataValidationResult& Result,
		const TMap<FGameplayTag, float>& Magnitudes, const TCHAR* FieldName)
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
		const UProjectTagDefinition* ProjectTagConfig = UProjectTagDefinition::GetDefaultDefinition();
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

	/** 빈 칸과 같은 종류의 조각이 두 번 들어간 경우를 막는다. 조회는 앞의 조각만 찾기 때문이다. */
	void ValidateFragmentList(FDataValidationContext& Context, EDataValidationResult& Result, const UItemDefinition& ItemDefinition)
	{
		TSet<const UScriptStruct*> FragmentTypes;
		for (const TInstancedStruct<FItemFragment>& Fragment : ItemDefinition.Fragments)
		{
			const UScriptStruct* FragmentType = Fragment.GetScriptStruct();
			if (!FragmentType)
			{
				MarkItemInvalid(Context, Result, NSLOCTEXT("ItemDefinition", "EmptyFragment", "Fragments contains an empty entry."));
				continue;
			}

			bool bAlreadyAdded = false;
			FragmentTypes.Add(FragmentType, &bAlreadyAdded);
			if (bAlreadyAdded)
			{
				MarkItemInvalid(Context, Result, FText::Format(
					NSLOCTEXT("ItemDefinition", "DuplicateFragment", "Fragments contains {0} more than once."),
					FragmentType->GetDisplayNameText()));
			}
		}
	}

	void ValidateConsumableFragment(FDataValidationContext& Context, EDataValidationResult& Result, const UItemDefinition& ItemDefinition)
	{
		const UProjectTagDefinition* ProjectTagConfig = UProjectTagDefinition::GetDefaultDefinition();
		const FGameplayTag ConsumableTypeTag = ProjectTagConfig
			? ProjectTagConfig->GetItemConsumableTypeTag()
			: PdGameplayTags::Item_Consumable;
		const bool bConsumableByTag = ItemDefinition.IsConsumableDefinition(ConsumableTypeTag);
		const FItemFragment_Consumable* Consumable = ItemDefinition.FindFragment<FItemFragment_Consumable>();
		if (!Consumable)
		{
			if (bConsumableByTag)
			{
				MarkItemInvalid(Context, Result, NSLOCTEXT("ItemDefinition", "MissingConsumableFragment", "Consumable items require the Consumable fragment."));
			}
			return;
		}

		if (Consumable->QuantityToConsume < 1)
		{
			MarkItemInvalid(Context, Result, NSLOCTEXT("ItemDefinition", "InvalidQuantityToConsume", "Consumable.QuantityToConsume must be at least 1."));
		}

		if (!Consumable->ConsumeGameplayEffectClass)
		{
			MarkItemInvalid(Context, Result, NSLOCTEXT("ItemDefinition", "MissingConsumableEffect", "The Consumable fragment requires ConsumeGameplayEffectClass."));
		}

		if (!bConsumableByTag)
		{
			Context.AddWarning(NSLOCTEXT("ItemDefinition", "ConsumableFragmentWithoutTag", "The Consumable fragment is set, but IdTag does not match the consumable item type. Quick slot usage will not treat this as a consumable."));
		}

		ValidateMagnitudeMap(Context, Result, Consumable->Map_Consume_Magnitude, TEXT("Consumable.Map_Consume_Magnitude"));
	}

	void ValidateDropFragment(FDataValidationContext& Context, EDataValidationResult& Result, const FItemFragment_Drop& Drop)
	{
		if (!FMath::IsFinite(Drop.DropRate))
		{
			MarkItemInvalid(Context, Result, NSLOCTEXT("ItemDefinition", "InvalidDropRateFinite", "Drop.DropRate must be finite."));
		}
		else if (Drop.DropRate < 0.0f)
		{
			MarkItemInvalid(Context, Result, NSLOCTEXT("ItemDefinition", "InvalidDropRateNegative", "Drop.DropRate cannot be negative."));
		}
		else if (FMath::IsNearlyZero(Drop.DropRate))
		{
			Context.AddWarning(NSLOCTEXT("ItemDefinition", "ZeroRewardChestDropRate", "Drop.DropRate is zero. Reward chests will not select this item; remove the Drop fragment instead."));
		}
	}

	void ValidateItemAimCameraSettings(FDataValidationContext& Context, EDataValidationResult& Result,
		const FWeaponAimCameraSettings& CameraSettings, const TCHAR* FieldPrefix)
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

	void ValidateBowFragment(FDataValidationContext& Context, EDataValidationResult& Result, const FItemFragment_Bow& Bow)
	{
		if (!Bow.ArrowActorClass)
		{
			MarkItemInvalid(Context, Result, NSLOCTEXT("ItemDefinition", "BowMissingArrowClass", "The Bow fragment requires Bow.ArrowActorClass."));
		}

		if (!FMath::IsFinite(Bow.TraceRange) || Bow.TraceRange <= 0.0f)
		{
			MarkItemInvalid(Context, Result, NSLOCTEXT("ItemDefinition", "InvalidBowTraceRange", "Bow.TraceRange must be a positive finite value."));
		}

		if (!FMath::IsFinite(Bow.MaxAcceptedServerLaunchStartDistance) || Bow.MaxAcceptedServerLaunchStartDistance < 0.0f)
		{
			MarkItemInvalid(Context, Result, NSLOCTEXT("ItemDefinition", "InvalidBowServerLaunchDistance", "Bow.MaxAcceptedServerLaunchStartDistance must be a non-negative finite value."));
		}

		if (!FMath::IsFinite(Bow.MinimumDrawDuration) || Bow.MinimumDrawDuration <= 0.0f)
		{
			MarkItemInvalid(Context, Result, NSLOCTEXT("ItemDefinition", "InvalidBowMinimumDrawDuration", "Bow.MinimumDrawDuration must be a positive finite value."));
		}

		if (!FMath::IsFinite(Bow.FireInterval) || Bow.FireInterval <= 0.0f)
		{
			MarkItemInvalid(Context, Result, NSLOCTEXT("ItemDefinition", "InvalidBowFireInterval", "Bow.FireInterval must be a positive finite value."));
		}

		if (Bow.TraceObjectTypes.IsEmpty())
		{
			Context.AddWarning(NSLOCTEXT("ItemDefinition", "BowEmptyTraceObjectTypes", "Bow.TraceObjectTypes is empty. Bow aim and launch traces may not hit anything."));
		}

		ValidateSoftObjectReference(Context, Result, Bow.WeaponMontage, TEXT("Bow.WeaponMontage"));
	}

	void ValidateGunFragment(FDataValidationContext& Context, EDataValidationResult& Result, const FItemFragment_Gun& Gun)
	{
		if (!FMath::IsFinite(Gun.TraceRange) || Gun.TraceRange <= 0.0f)
		{
			MarkItemInvalid(Context, Result, NSLOCTEXT("ItemDefinition", "InvalidGunTraceRange", "Gun.TraceRange must be a positive finite value."));
		}

		if (!FMath::IsFinite(Gun.TraceRadius) || Gun.TraceRadius < 0.0f)
		{
			MarkItemInvalid(Context, Result, NSLOCTEXT("ItemDefinition", "InvalidGunTraceRadius", "Gun.TraceRadius must be a non-negative finite value."));
		}

		if (!FMath::IsFinite(Gun.FireInterval) || Gun.FireInterval <= 0.0f)
		{
			MarkItemInvalid(Context, Result, NSLOCTEXT("ItemDefinition", "InvalidGunFireInterval", "Gun.FireInterval must be positive."));
		}

		const FVector2D& ImpactDecalSizeRange = Gun.ImpactDecalSizeRange;
		if (!FMath::IsFinite(ImpactDecalSizeRange.X) || !FMath::IsFinite(ImpactDecalSizeRange.Y)
			|| ImpactDecalSizeRange.X < 0.0f || ImpactDecalSizeRange.Y < 0.0f)
		{
			MarkItemInvalid(Context, Result, NSLOCTEXT("ItemDefinition", "InvalidGunImpactDecalSizeRange", "Gun.ImpactDecalSizeRange must contain non-negative finite values."));
		}
		else if (ImpactDecalSizeRange.X > ImpactDecalSizeRange.Y)
		{
			MarkItemInvalid(Context, Result, NSLOCTEXT("ItemDefinition", "InvertedGunImpactDecalSizeRange", "Gun.ImpactDecalSizeRange minimum cannot be greater than maximum."));
		}

		if (!FMath::IsFinite(Gun.ImpactDecalDepth) || Gun.ImpactDecalDepth < 0.0f)
		{
			MarkItemInvalid(Context, Result, NSLOCTEXT("ItemDefinition", "InvalidGunImpactDecalDepth", "Gun.ImpactDecalDepth must be a non-negative finite value."));
		}

		if (!FMath::IsFinite(Gun.ImpactDecalLifeSpan) || Gun.ImpactDecalLifeSpan < 0.0f)
		{
			MarkItemInvalid(Context, Result, NSLOCTEXT("ItemDefinition", "InvalidGunImpactDecalLifeSpan", "Gun.ImpactDecalLifeSpan must be a non-negative finite value."));
		}

		if (Gun.ImpactDecalRotationOffset.ContainsNaN())
		{
			MarkItemInvalid(Context, Result, NSLOCTEXT("ItemDefinition", "InvalidGunImpactDecalRotation", "Gun.ImpactDecalRotationOffset contains NaN."));
		}

		ValidateSoftObjectReference(Context, Result, Gun.ImpactDecalMaterial, TEXT("Gun.ImpactDecalMaterial"));

		if (Gun.TraceObjectTypes.IsEmpty())
		{
			Context.AddWarning(NSLOCTEXT("ItemDefinition", "GunEmptyTraceObjectTypes", "Gun.TraceObjectTypes is empty. Gun traces may not hit anything."));
		}
	}

	/** 무기 공통 조각과, 그 위에 얹는 활·총 조각을 함께 본다. */
	void ValidateWeaponFragments(FDataValidationContext& Context, EDataValidationResult& Result, const UItemDefinition& ItemDefinition)
	{
		const UProjectTagDefinition* ProjectTagConfig = UProjectTagDefinition::GetDefaultDefinition();
		const FGameplayTag WeaponTypeTag = ProjectTagConfig
			? ProjectTagConfig->GetItemWeaponTypeTag()
			: PdGameplayTags::Item_Weapon;
		const bool bWeaponByTag = ItemDefinition.MatchesItemType(WeaponTypeTag);
		const FItemFragment_Weapon* Weapon = ItemDefinition.FindFragment<FItemFragment_Weapon>();
		const FItemFragment_Bow* Bow = ItemDefinition.FindFragment<FItemFragment_Bow>();
		const FItemFragment_Gun* Gun = ItemDefinition.FindFragment<FItemFragment_Gun>();

		if (!Weapon)
		{
			if (bWeaponByTag)
			{
				MarkItemInvalid(Context, Result, NSLOCTEXT("ItemDefinition", "MissingWeaponFragment", "Weapon items require the Weapon fragment."));
			}

			if (Bow || Gun)
			{
				MarkItemInvalid(Context, Result, NSLOCTEXT("ItemDefinition", "RangedFragmentWithoutWeapon", "Bow and Gun fragments require the Weapon fragment."));
			}
			return;
		}

		if (!bWeaponByTag)
		{
			Context.AddWarning(NSLOCTEXT("ItemDefinition", "WeaponFragmentWithoutWeaponTag", "The Weapon fragment is set, but IdTag does not match the weapon item type. Inventory filtering may not show it as a weapon."));
		}

		ValidateSoftClassReference(Context, Result, Weapon->Equip.ActorClass, TEXT("Weapon.Equip.ActorClass"));
		ValidateSoftObjectReference(Context, Result, Weapon->Equip.EquipMontage, TEXT("Weapon.Equip.EquipMontage"));
		ValidateSoftObjectReference(Context, Result, Weapon->Equip.UnequipMontage, TEXT("Weapon.Equip.UnequipMontage"));
		ValidateSoftClassReference(Context, Result, Weapon->Equip.AnimLayer, TEXT("Weapon.Equip.AnimLayer"));
		ValidateSoftObjectReference(Context, Result, Weapon->Attack.AttackMontage, TEXT("Weapon.Attack.AttackMontage"));
		ValidateSoftObjectReference(Context, Result, Weapon->HitReact.HitReactMontage, TEXT("Weapon.HitReact.HitReactMontage"));

		if (!FMath::IsFinite(Weapon->Attack.StaminaCost) || Weapon->Attack.StaminaCost < 0.0f)
		{
			MarkItemInvalid(Context, Result, NSLOCTEXT("ItemDefinition", "InvalidWeaponAttackStaminaCost", "Weapon.Attack.StaminaCost must be a non-negative finite value."));
		}

		if (!FMath::IsFinite(Weapon->Movement.MeleeEquippedMovementSpeedMultiplier)
			|| Weapon->Movement.MeleeEquippedMovementSpeedMultiplier <= 0.0f)
		{
			MarkItemInvalid(Context, Result, NSLOCTEXT("ItemDefinition", "InvalidMeleeMovementSpeedMultiplier", "Weapon.Movement.MeleeEquippedMovementSpeedMultiplier must be a positive finite value."));
		}

		if (Weapon->Equip.ActorClass.IsNull())
		{
			MarkItemInvalid(Context, Result, NSLOCTEXT("ItemDefinition", "WeaponMissingActorClass", "The Weapon fragment requires Weapon.Equip.ActorClass."));
		}

		if (Weapon->Equip.AttachSocketName == EWeaponSocketName::None)
		{
			Context.AddWarning(NSLOCTEXT("ItemDefinition", "WeaponMissingAttachSocket", "Weapon.Equip.AttachSocketName is None. The weapon may attach to the wrong place."));
		}

		if (Weapon->Aim.CrosshairWidgetTag.IsValid() && !Weapon->Aim.bSupportsInput)
		{
			Context.AddWarning(NSLOCTEXT("ItemDefinition", "AimCrosshairWithoutInput", "Weapon.Aim.CrosshairWidgetTag is set, but bSupportsInput is false."));
		}

		if (Weapon->Aim.bSupportsInput && !Weapon->Aim.CrosshairWidgetTag.IsValid())
		{
			Context.AddWarning(NSLOCTEXT("ItemDefinition", "AimInputWithoutCrosshair", "Weapon.Aim.bSupportsInput is true, but CrosshairWidgetTag is not set."));
		}

		if (!FMath::IsFinite(Weapon->Aim.MaxAcceptedServerViewDistance) || Weapon->Aim.MaxAcceptedServerViewDistance < 0.0f)
		{
			MarkItemInvalid(Context, Result, NSLOCTEXT("ItemDefinition", "InvalidAimServerViewDistance", "Weapon.Aim.MaxAcceptedServerViewDistance must be a non-negative finite value."));
		}

		ValidateItemAimCameraSettings(Context, Result, Weapon->Aim.CameraSettings, TEXT("Weapon.Aim.CameraSettings"));

		if (!FMath::IsFinite(Weapon->AI.RangedTargetLockDelay) || Weapon->AI.RangedTargetLockDelay < 0.0f)
		{
			MarkItemInvalid(Context, Result, NSLOCTEXT("ItemDefinition", "InvalidAiRangedTargetLockDelay", "Weapon.AI.RangedTargetLockDelay must be a non-negative finite value."));
		}

		if (Bow)
		{
			ValidateBowFragment(Context, Result, *Bow);
		}

		if (Gun)
		{
			ValidateGunFragment(Context, Result, *Gun);
		}

		if (Bow && Gun)
		{
			Context.AddWarning(NSLOCTEXT("ItemDefinition", "BowAndGunDataSet", "Both Bow and Gun fragments are set. Confirm that the weapon actor is expected to use both."));
		}
	}
#endif

#if WITH_EDITORONLY_DATA
	/** 옛 애셋에서 기본값과 다른 값이 하나라도 있는 묶음만 조각으로 옮긴다. */
	template <typename StructType>
	bool DiffersFromDefault(const StructType& Value)
	{
		const StructType DefaultValue;
		return !StructType::StaticStruct()->CompareScriptStruct(&Value, &DefaultValue, PPF_None);
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

void UItemDefinition::PostLoad()
{
#if WITH_EDITORONLY_DATA
	// 부모의 PostLoad가 애셋 번들을 다시 계산하므로, 옮긴 조각이 번들에 들어가도록 먼저 옮긴다.
	MoveLegacyDataToFragments();
#endif

	Super::PostLoad();
}

#if WITH_EDITORONLY_DATA
void UItemDefinition::UpdateAssetBundleData()
{
	Super::UpdateAssetBundleData();
	if (!UAssetManager::IsInitialized())
	{
		return;
	}

	// 엔진은 AssetBundles 메타데이터를 리플렉션 프로퍼티에서만 찾아서 조각(TInstancedStruct) 안까지 들어가지 않는다.
	// 무기 표현 애셋은 직접 번들에 넣는다.
	TArray<FSoftObjectPath> PresentationAssetPaths;
	GetWeaponPresentationAssetPaths(PresentationAssetPaths);
	AssetBundleData.AddBundleAssetsTruncated(GetWeaponPresentationBundleName(), PresentationAssetPaths);
}

void UItemDefinition::MoveLegacyDataToFragments()
{
	// 새 구조로 저장한 애셋은 조각을 이미 가지고 있다. 옛 애셋은 모두 드롭 데이터가 있어서 옮기면 조각이 하나 이상 생긴다.
	if (!Fragments.IsEmpty())
	{
		return;
	}

	if (!Map_Stat_Magnitude_DEPRECATED.IsEmpty())
	{
		FItemFragment_Stat Stat;
		Stat.Map_Stat_Magnitude = MoveTemp(Map_Stat_Magnitude_DEPRECATED);
		Fragments.Add(TInstancedStruct<FItemFragment>::Make<FItemFragment_Stat>(Stat));
	}

	if (ConsumeGameplayEffectClass_DEPRECATED || !Map_Consume_Magnitude_DEPRECATED.IsEmpty() || QuantityToConsume_DEPRECATED != 1)
	{
		FItemFragment_Consumable Consumable;
		Consumable.ConsumeGameplayEffectClass = ConsumeGameplayEffectClass_DEPRECATED;
		Consumable.Map_Consume_Magnitude = MoveTemp(Map_Consume_Magnitude_DEPRECATED);
		Consumable.QuantityToConsume = QuantityToConsume_DEPRECATED;
		Fragments.Add(TInstancedStruct<FItemFragment>::Make<FItemFragment_Consumable>(Consumable));
	}

	if (bCanDropFromRewardChest_DEPRECATED)
	{
		FItemFragment_Drop Drop;
		Drop.DropRate = DropRate_DEPRECATED;
		Fragments.Add(TInstancedStruct<FItemFragment>::Make<FItemFragment_Drop>(Drop));
	}

	const FWeaponDefinitionData& LegacyWeapon = WeaponData_DEPRECATED;
	if (DiffersFromDefault(LegacyWeapon.Equip) || DiffersFromDefault(LegacyWeapon.Attack) || DiffersFromDefault(LegacyWeapon.Movement)
		|| DiffersFromDefault(LegacyWeapon.HitReact) || DiffersFromDefault(LegacyWeapon.Aim) || DiffersFromDefault(LegacyWeapon.AI))
	{
		FItemFragment_Weapon Weapon;
		Weapon.Equip = LegacyWeapon.Equip;
		Weapon.Attack = LegacyWeapon.Attack;
		Weapon.Movement = LegacyWeapon.Movement;
		Weapon.HitReact = LegacyWeapon.HitReact;
		Weapon.Aim = LegacyWeapon.Aim;
		Weapon.AI = LegacyWeapon.AI;
		Fragments.Add(TInstancedStruct<FItemFragment>::Make<FItemFragment_Weapon>(Weapon));
	}

	if (DiffersFromDefault(LegacyWeapon.Bow))
	{
		Fragments.Add(TInstancedStruct<FItemFragment>::Make<FItemFragment_Bow>(LegacyWeapon.Bow));
	}

	if (DiffersFromDefault(LegacyWeapon.Gun))
	{
		Fragments.Add(TInstancedStruct<FItemFragment>::Make<FItemFragment_Gun>(LegacyWeapon.Gun));
	}
}
#endif

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

	if (const FItemFragment_Weapon* Weapon = FindFragment<FItemFragment_Weapon>())
	{
		AddPath(Weapon->Equip.ActorClass.ToSoftObjectPath());
		AddPath(Weapon->Equip.EquipMontage.ToSoftObjectPath());
		AddPath(Weapon->Equip.UnequipMontage.ToSoftObjectPath());
		AddPath(Weapon->Equip.AnimLayer.ToSoftObjectPath());
		AddPath(Weapon->Attack.AttackMontage.ToSoftObjectPath());
		AddPath(Weapon->HitReact.HitReactMontage.ToSoftObjectPath());
	}

	if (const FItemFragment_Bow* Bow = FindFragment<FItemFragment_Bow>())
	{
		AddPath(Bow->WeaponMontage.ToSoftObjectPath());
	}

	if (const FItemFragment_Gun* Gun = FindFragment<FItemFragment_Gun>())
	{
		AddPath(Gun->ImpactDecalMaterial.ToSoftObjectPath());
	}
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
	ValidateItemShopData(Context, Result, ShopData);
	ValidateFragmentList(Context, Result, *this);

	if (const FItemFragment_Stat* Stat = FindFragment<FItemFragment_Stat>())
	{
		ValidateMagnitudeMap(Context, Result, Stat->Map_Stat_Magnitude, TEXT("Stat.Map_Stat_Magnitude"));
	}

	if (const FItemFragment_Drop* Drop = FindFragment<FItemFragment_Drop>())
	{
		ValidateDropFragment(Context, Result, *Drop);
	}

	ValidateConsumableFragment(Context, Result, *this);
	ValidateWeaponFragments(Context, Result, *this);

	return Result;
}
#endif

bool UItemDefinition::MatchesItemType(const FGameplayTag ItemTypeTag) const
{
	return IdTag.IsValid() && ItemTypeTag.IsValid() && IdTag.MatchesTag(ItemTypeTag);
}

bool UItemDefinition::IsWeaponDefinition(const FGameplayTag WeaponTypeTag) const
{
	return MatchesItemType(WeaponTypeTag) || HasFragment<FItemFragment_Weapon>();
}

bool UItemDefinition::IsConsumableDefinition(const FGameplayTag ConsumableTypeTag) const
{
	return MatchesItemType(ConsumableTypeTag);
}
