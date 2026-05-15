#pragma once

#include "CoreMinimal.h"
#include "Common/WeaponDefinitionData.h"
#include "Engine/DataAsset.h"
#include "ItemDefinition.generated.h"

class AActor;
class AWeaponBase;
class UAnimInstance;
class UAnimMontage;
class UMaterialInterface;
class UTexture2D;

DECLARE_LOG_CATEGORY_EXTERN(ItemDefinitionLog, Log, All);

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API UItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UItemDefinition();
	virtual void PostLoad() override;
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	bool HasBowData() const;

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item")
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item")
	TSoftObjectPtr<UTexture2D> IconTexture = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item")
	FGameplayTag IdTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item")
	TMap<FGameplayTag, float> Map_Stat_Magnitude;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item")
	int Tier;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon")
	FWeaponDefinitionData WeaponData;

private:
	void MigrateDeprecatedWeaponData();
	bool HasDeprecatedWeaponData() const;

	UPROPERTY()
	bool bHasMigratedDeprecatedWeaponData = false;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use WeaponData.Equip.ActorClass."))
	TSoftClassPtr<AWeaponBase> EquippedActorClass;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use WeaponData.Equip.AttachSocketName."))
	FName EquipAttachSocketName = NAME_None;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use WeaponData.Equip.EquipMontage."))
	TSoftObjectPtr<UAnimMontage> EquipMontage = nullptr;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use WeaponData.Equip.UnequipMontage."))
	TSoftObjectPtr<UAnimMontage> UnequipMontage = nullptr;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use WeaponData.Attack.AttackMontage."))
	TSoftObjectPtr<UAnimMontage> AttackMontage = nullptr;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use WeaponData.Attack.bAllowMovementDuringAttack."))
	bool bAllowMovementDuringAttack = true;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use WeaponData.HitReact.HitReactMontage."))
	TSoftObjectPtr<UAnimMontage> HitReactMontage = nullptr;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use WeaponData.Equip.AnimLayer."))
	TSoftClassPtr<UAnimInstance> EquipAnimLayer;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use WeaponData.Aim.bSupportsInput."))
	bool bSupportsAimInput = false;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use WeaponData.Aim.CameraSettings."))
	FWeaponAimCameraSettings AimCameraSettings;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use WeaponData.Bow.ArrowActorClass."))
	TSubclassOf<AActor> ArrowActorClass;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use WeaponData.Bow.ArrowAttachSocketName."))
	FName ArrowAttachSocketName = NAME_None;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use WeaponData.Bow.TraceRange."))
	float BowTraceRange = 7000.0f;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use WeaponData.Bow.TraceObjectTypes."))
	TArray<TEnumAsByte<EObjectTypeQuery>> BowTraceObjectTypes;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use WeaponData.Bow.AimTraceDebugDrawType."))
	TEnumAsByte<EDrawDebugTrace::Type> BowAimTraceDebugDrawType = EDrawDebugTrace::None;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use WeaponData.Bow.LaunchTraceDebugDrawType."))
	TEnumAsByte<EDrawDebugTrace::Type> BowLaunchTraceDebugDrawType = EDrawDebugTrace::ForDuration;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use WeaponData.Bow.WeaponMontage."))
	TSoftObjectPtr<UAnimMontage> BowWeaponMontage = nullptr;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use WeaponData.Bow.AttackResumeSectionName."))
	FName BowAttackResumeSectionName = TEXT("Skip");

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use WeaponData.Gun.TraceRange."))
	float GunTraceRange = 10000.0f;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use WeaponData.Gun.TraceRadius."))
	float GunTraceRadius = 5.0f;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use WeaponData.Gun.TraceObjectTypes."))
	TArray<TEnumAsByte<EObjectTypeQuery>> GunTraceObjectTypes;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use WeaponData.Gun.AimTraceDebugDrawType."))
	TEnumAsByte<EDrawDebugTrace::Type> GunAimTraceDebugDrawType = EDrawDebugTrace::None;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use WeaponData.Gun.ShotTraceDebugDrawType."))
	TEnumAsByte<EDrawDebugTrace::Type> GunShotTraceDebugDrawType = EDrawDebugTrace::ForDuration;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use WeaponData.Gun.bEnableAutomaticFire."))
	bool bEnableAutomaticFire = false;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use WeaponData.Gun.FireInterval."))
	float AutomaticFireInterval = 0.12f;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use WeaponData.Gun.MuzzleFlashCueTag."))
	FGameplayTag GunMuzzleFlashCueTag;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use WeaponData.Gun.MuzzleSocketName."))
	FName GunMuzzleSocketName = TEXT("Muzzle");

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use WeaponData.Gun.ImpactDecalMaterial."))
	TSoftObjectPtr<UMaterialInterface> GunImpactDecalMaterial = nullptr;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use WeaponData.Gun.ImpactDecalObjectTypes."))
	TArray<TEnumAsByte<EObjectTypeQuery>> GunImpactDecalObjectTypes;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use WeaponData.Gun.ImpactDecalSizeRange."))
	FVector2D GunImpactDecalSizeRange = FVector2D(3.0f, 7.0f);

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use WeaponData.Gun.ImpactDecalDepth."))
	float GunImpactDecalDepth = 5.0f;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use WeaponData.Gun.ImpactDecalRotationOffset."))
	FRotator GunImpactDecalRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use WeaponData.Gun.ImpactDecalLifeSpan."))
	float GunImpactDecalLifeSpan = 10.0f;
};
