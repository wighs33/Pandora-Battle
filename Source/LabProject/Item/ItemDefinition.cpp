#include "ItemDefinition.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(ItemDefinition)

DEFINE_LOG_CATEGORY(ItemDefinitionLog);

UItemDefinition::UItemDefinition()
{
}

void UItemDefinition::PostLoad()
{
	Super::PostLoad();

	if (!bHasMigratedDeprecatedWeaponData)
	{
		MigrateDeprecatedWeaponData();
		bHasMigratedDeprecatedWeaponData = true;
	}
}

FPrimaryAssetId UItemDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("ItemDefinition"), GetFName());
}

bool UItemDefinition::HasBowData() const
{
	return WeaponData.Bow.HasAnyData();
}

bool UItemDefinition::HasDeprecatedWeaponData() const
{
	return !EquippedActorClass.IsNull()
		|| !EquipAttachSocketName.IsNone()
		|| !EquipMontage.IsNull()
		|| !UnequipMontage.IsNull()
		|| !AttackMontage.IsNull()
		|| !bAllowMovementDuringAttack
		|| !HitReactMontage.IsNull()
		|| !EquipAnimLayer.IsNull()
		|| bSupportsAimInput
		|| ArrowActorClass != nullptr
		|| !ArrowAttachSocketName.IsNone()
		|| !BowTraceObjectTypes.IsEmpty()
		|| BowTraceRange != FBowWeaponDefinitionData().TraceRange
		|| BowAimTraceDebugDrawType != FBowWeaponDefinitionData().AimTraceDebugDrawType
		|| BowLaunchTraceDebugDrawType != FBowWeaponDefinitionData().LaunchTraceDebugDrawType
		|| !BowWeaponMontage.IsNull()
		|| BowAttackResumeSectionName != FBowWeaponDefinitionData().AttackResumeSectionName
		|| GunTraceRange != FGunWeaponDefinitionData().TraceRange
		|| GunTraceRadius != FGunWeaponDefinitionData().TraceRadius
		|| !GunTraceObjectTypes.IsEmpty()
		|| GunAimTraceDebugDrawType != FGunWeaponDefinitionData().AimTraceDebugDrawType
		|| GunShotTraceDebugDrawType != FGunWeaponDefinitionData().ShotTraceDebugDrawType
		|| bEnableAutomaticFire
		|| AutomaticFireInterval != FGunWeaponDefinitionData().FireInterval
		|| GunMuzzleFlashCueTag.IsValid()
		|| (!GunMuzzleSocketName.IsNone() && GunMuzzleSocketName != FGunWeaponDefinitionData().GetResolvedMuzzleSocketName())
		|| !GunImpactDecalMaterial.IsNull()
		|| !GunImpactDecalObjectTypes.IsEmpty()
		|| GunImpactDecalSizeRange != FGunWeaponDefinitionData().ImpactDecalSizeRange
		|| GunImpactDecalDepth != FGunWeaponDefinitionData().ImpactDecalDepth
		|| GunImpactDecalRotationOffset != FGunWeaponDefinitionData().ImpactDecalRotationOffset
		|| GunImpactDecalLifeSpan != FGunWeaponDefinitionData().ImpactDecalLifeSpan;
}

void UItemDefinition::MigrateDeprecatedWeaponData()
{
	if (!HasDeprecatedWeaponData())
	{
		return;
	}

	WeaponData.Equip.ActorClass = EquippedActorClass;
	WeaponData.Equip.AttachSocketName = PdWeaponSockets::FromName(EquipAttachSocketName);
	WeaponData.Equip.EquipMontage = EquipMontage;
	WeaponData.Equip.UnequipMontage = UnequipMontage;
	WeaponData.Equip.AnimLayer = EquipAnimLayer;

	WeaponData.Attack.AttackMontage = AttackMontage;
	WeaponData.Attack.bAllowMovementDuringAttack = bAllowMovementDuringAttack;
	WeaponData.HitReact.HitReactMontage = HitReactMontage;

	WeaponData.Aim.bSupportsInput = bSupportsAimInput;
	WeaponData.Aim.CameraSettings = AimCameraSettings;

	WeaponData.Bow.ArrowActorClass = ArrowActorClass;
	if (!ArrowAttachSocketName.IsNone())
	{
		WeaponData.Bow.ArrowAttachSocketName = PdWeaponSockets::FromName(ArrowAttachSocketName);
	}
	WeaponData.Bow.TraceRange = BowTraceRange;
	if (!BowTraceObjectTypes.IsEmpty())
	{
		WeaponData.Bow.TraceObjectTypes = BowTraceObjectTypes;
	}
	WeaponData.Bow.AimTraceDebugDrawType = BowAimTraceDebugDrawType;
	WeaponData.Bow.LaunchTraceDebugDrawType = BowLaunchTraceDebugDrawType;
	WeaponData.Bow.WeaponMontage = BowWeaponMontage;
	WeaponData.Bow.AttackResumeSectionName = BowAttackResumeSectionName;

	WeaponData.Gun.TraceRange = GunTraceRange;
	WeaponData.Gun.TraceRadius = GunTraceRadius;
	if (!GunTraceObjectTypes.IsEmpty())
	{
		WeaponData.Gun.TraceObjectTypes = GunTraceObjectTypes;
	}
	WeaponData.Gun.AimTraceDebugDrawType = GunAimTraceDebugDrawType;
	WeaponData.Gun.ShotTraceDebugDrawType = GunShotTraceDebugDrawType;
	WeaponData.Gun.bEnableAutomaticFire = bEnableAutomaticFire;
	WeaponData.Gun.FireInterval = AutomaticFireInterval;
	WeaponData.Gun.MuzzleFlashCueTag = GunMuzzleFlashCueTag;
	WeaponData.Gun.MuzzleSocketName = PdWeaponSockets::FromName(GunMuzzleSocketName);
	WeaponData.Gun.ImpactDecalMaterial = GunImpactDecalMaterial;
	if (!GunImpactDecalObjectTypes.IsEmpty())
	{
		WeaponData.Gun.ImpactDecalObjectTypes = GunImpactDecalObjectTypes;
	}
	WeaponData.Gun.ImpactDecalSizeRange = GunImpactDecalSizeRange;
	WeaponData.Gun.ImpactDecalDepth = GunImpactDecalDepth;
	WeaponData.Gun.ImpactDecalRotationOffset = GunImpactDecalRotationOffset;
	WeaponData.Gun.ImpactDecalLifeSpan = GunImpactDecalLifeSpan;
}
