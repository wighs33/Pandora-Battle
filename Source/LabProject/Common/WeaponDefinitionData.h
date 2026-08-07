#pragma once

#include "CoreMinimal.h"
#include "Common/CollisionChannels.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UObject/SoftObjectPtr.h"
#include "WeaponDefinitionData.generated.h"

class AActor;
class AWeaponBase;
class UAnimInstance;
class UAnimMontage;
class UMaterialInterface;
class UNiagaraSystem;

UENUM(BlueprintType)
enum class EWeaponSocketName : uint8
{
	None UMETA(DisplayName = "None"),
	Muzzle UMETA(DisplayName = "Muzzle"),
	Socket_Arrow UMETA(DisplayName = "Socket_Arrow"),
	Socket_Axe UMETA(DisplayName = "Socket_Axe"),
	Socket_Bow UMETA(DisplayName = "Socket_Bow"),
	Socket_Dagger UMETA(DisplayName = "Socket_Dagger"),
	Socket_hand_r UMETA(DisplayName = "Socket_hand_r"),
	Socket_Pistol UMETA(DisplayName = "Socket_Pistol"),
	Socket_Rifle UMETA(DisplayName = "Socket_Rifle")
};

namespace PdWeaponSockets
{
	inline FName ToName(EWeaponSocketName SocketName)
	{
		switch (SocketName)
		{
		case EWeaponSocketName::Muzzle:
			return FName(TEXT("Muzzle"));
		case EWeaponSocketName::Socket_Arrow:
			return FName(TEXT("Socket_Arrow"));
		case EWeaponSocketName::Socket_Axe:
			return FName(TEXT("Socket_Axe"));
		case EWeaponSocketName::Socket_Bow:
			return FName(TEXT("Socket_Bow"));
		case EWeaponSocketName::Socket_Dagger:
			return FName(TEXT("Socket_Dagger"));
		case EWeaponSocketName::Socket_hand_r:
			return FName(TEXT("Socket_hand_r"));
		case EWeaponSocketName::Socket_Pistol:
			return FName(TEXT("Socket_Pistol"));
		case EWeaponSocketName::Socket_Rifle:
			return FName(TEXT("Socket_Rifle"));
		case EWeaponSocketName::None:
		default:
			return NAME_None;
		}
	}

	inline EWeaponSocketName FromName(FName SocketName)
	{
		if (SocketName == FName(TEXT("Muzzle")))
		{
			return EWeaponSocketName::Muzzle;
		}

		if (SocketName == FName(TEXT("Socket_Arrow")))
		{
			return EWeaponSocketName::Socket_Arrow;
		}

		if (SocketName == FName(TEXT("Socket_Axe")))
		{
			return EWeaponSocketName::Socket_Axe;
		}

		if (SocketName == FName(TEXT("Socket_Bow")))
		{
			return EWeaponSocketName::Socket_Bow;
		}

		if (SocketName == FName(TEXT("Socket_Dagger")))
		{
			return EWeaponSocketName::Socket_Dagger;
		}

		if (SocketName == FName(TEXT("Socket_hand_r")))
		{
			return EWeaponSocketName::Socket_hand_r;
		}

		if (SocketName == FName(TEXT("Socket_Pistol")))
		{
			return EWeaponSocketName::Socket_Pistol;
		}

		if (SocketName == FName(TEXT("Socket_Rifle")))
		{
			return EWeaponSocketName::Socket_Rifle;
		}

		return EWeaponSocketName::None;
	}
}

USTRUCT(BlueprintType)
struct LABPROJECT_API FWeaponAimCameraSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon|Aim|Camera")
	float TargetFOV = 50.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon|Aim|Camera")
	FVector TargetBoomSocketOffset = FVector(30.0f, 200.0f, 65.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon|Aim|Camera")
	FRotator TargetCameraRotation = FRotator(0.0f, -20.0f, 0.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon|Aim|Camera", meta = (DisplayName = "Camera Transition Speed"))
	float InterpSpeed = 10.0f;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FWeaponEquipDefinitionData
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon|Equip")
	TSoftClassPtr<AWeaponBase> ActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon|Equip")
	EWeaponSocketName AttachSocketName = EWeaponSocketName::None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon|Equip")
	TSoftObjectPtr<UAnimMontage> EquipMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon|Equip")
	TSoftObjectPtr<UAnimMontage> UnequipMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon|Equip")
	TSoftClassPtr<UAnimInstance> AnimLayer;

	bool HasAnyData() const
	{
		return !ActorClass.IsNull()
			|| AttachSocketName != EWeaponSocketName::None
			|| !EquipMontage.IsNull()
			|| !UnequipMontage.IsNull()
			|| !AnimLayer.IsNull();
	}

	FName GetResolvedAttachSocketName() const
	{
		return PdWeaponSockets::ToName(AttachSocketName);
	}
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FWeaponAttackDefinitionData
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon|Attack")
	TSoftObjectPtr<UAnimMontage> AttackMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon|Attack",
		meta = (DisplayName = "Combo Window Start Effect"))
	TObjectPtr<UNiagaraSystem> ComboWindowStartEffect = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon|Attack")
	bool bAllowMovementDuringAttack = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon|Attack",
		meta = (ClampMin = "0.0", UIMin = "0.0", DisplayName = "Stamina Cost",
			ToolTip = "Stamina consumed by each committed attack. Combo steps consume this amount again."))
	float StaminaCost = 10.0f;

	bool HasAnyData() const
	{
		return !AttackMontage.IsNull()
			|| ComboWindowStartEffect != nullptr
			|| !bAllowMovementDuringAttack
			|| !FMath::IsNearlyEqual(StaminaCost, 10.0f);
	}
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FWeaponMovementDefinitionData
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon|Movement",
		meta = (ClampMin = "0.01", UIMin = "0.01",
			DisplayName = "Melee Equipped Movement Speed Multiplier",
			ToolTip = "Movement speed multiplier applied while this non-aiming melee weapon is equipped. A value of 1.05 is five percent faster."))
	float MeleeEquippedMovementSpeedMultiplier = 1.05f;

	bool HasAnyData() const
	{
		return !FMath::IsNearlyEqual(
			MeleeEquippedMovementSpeedMultiplier,
			1.05f);
	}
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FWeaponHitReactDefinitionData
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon|HitReact")
	TSoftObjectPtr<UAnimMontage> HitReactMontage = nullptr;

	bool HasAnyData() const
	{
		return !HitReactMontage.IsNull();
	}
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FWeaponAimDefinitionData
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon|Aim")
	bool bSupportsInput = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon|Aim", meta = (Categories = "UI.Widget"))
	FGameplayTag CrosshairWidgetTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon|Aim")
	FWeaponAimCameraSettings CameraSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon|Aim|Validation", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float MaxAcceptedServerViewDistance = 0.0f;

	bool HasAnyData() const
	{
		return bSupportsInput
			|| CrosshairWidgetTag.IsValid();
	}
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FBowWeaponDefinitionData
{
	GENERATED_BODY()

	FBowWeaponDefinitionData()
	{
		ArrowAttachSocketName = GetDefaultArrowAttachSocket();
		TraceObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));
		TraceObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
		TraceObjectTypes.Add(UEngineTypes::ConvertToObjectType(LabCollisionChannels::HitableBody()));
	}

	static EWeaponSocketName GetDefaultArrowAttachSocket()
	{
		return EWeaponSocketName::Socket_Arrow;
	}

	FName GetResolvedArrowAttachSocketName() const
	{
		return PdWeaponSockets::ToName(ArrowAttachSocketName);
	}

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon|Bow|Projectile")
	TSubclassOf<AActor> ArrowActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon|Bow|Projectile")
	EWeaponSocketName ArrowAttachSocketName = EWeaponSocketName::Socket_Arrow;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon|Bow|Trace")
	float TraceRange = 7000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon|Bow|Validation", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float MaxAcceptedServerLaunchStartDistance = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon|Bow|Fire", meta = (ClampMin = "0.01", ForceUnits = "s", DisplayName = "Minimum Draw Duration", ToolTip = "Minimum server-authoritative time required before one arrow launch token is issued. Attack speed scales this duration."))
	float MinimumDrawDuration = 0.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon|Bow|Fire", meta = (ClampMin = "0.01", ForceUnits = "s", DisplayName = "Fire Interval", ToolTip = "Minimum server-authoritative time between successful arrow launches. Attack speed scales this interval."))
	float FireInterval = 0.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon|Bow|Trace")
	TArray<TEnumAsByte<EObjectTypeQuery>> TraceObjectTypes;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon|Bow|Trace", meta = (DisplayName = "Aim Trace Debug Draw"))
	TEnumAsByte<EDrawDebugTrace::Type> AimTraceDebugDrawType = EDrawDebugTrace::None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon|Bow|Animation")
	TSoftObjectPtr<UAnimMontage> WeaponMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon|Bow|Animation")
	FName AttackResumeSectionName = TEXT("Skip");

	bool HasAnyData() const
	{
		return ArrowActorClass != nullptr
			|| ArrowAttachSocketName != GetDefaultArrowAttachSocket()
			|| !WeaponMontage.IsNull();
	}
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FGunWeaponDefinitionData
{
	GENERATED_BODY()

	FGunWeaponDefinitionData()
	{
		TraceObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));
		TraceObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
		TraceObjectTypes.Add(UEngineTypes::ConvertToObjectType(LabCollisionChannels::HitableBody()));
		ImpactDecalObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));
	}

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon|Gun|Trace")
	float TraceRange = 10000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon|Gun|Trace")
	float TraceRadius = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon|Gun|Trace")
	TArray<TEnumAsByte<EObjectTypeQuery>> TraceObjectTypes;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon|Gun|Trace", meta = (DisplayName = "Aim Trace Debug Draw"))
	TEnumAsByte<EDrawDebugTrace::Type> AimTraceDebugDrawType = EDrawDebugTrace::None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon|Gun|Trace", meta = (DisplayName = "Shot Trace Debug Draw"))
	TEnumAsByte<EDrawDebugTrace::Type> ShotTraceDebugDrawType = EDrawDebugTrace::ForDuration;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon|Gun|Fire")
	bool bEnableAutomaticFire = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon|Gun|Fire", meta = (ClampMin = "0.01", ForceUnits = "s", DisplayName = "Fire Interval", ToolTip = "Minimum time between gun shots. Also used as the automatic fire timer interval."))
	float FireInterval = 0.12f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon|Gun|VFX", meta = (Categories = "GameplayCue"))
	FGameplayTag MuzzleFlashCueTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon|Gun|VFX")
	EWeaponSocketName MuzzleSocketName = EWeaponSocketName::Muzzle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon|Gun|Impact")
	TSoftObjectPtr<UMaterialInterface> ImpactDecalMaterial = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon|Gun|Impact")
	TArray<TEnumAsByte<EObjectTypeQuery>> ImpactDecalObjectTypes;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon|Gun|Impact", meta = (ClampMin = "0.0", ForceUnits = "cm", DisplayName = "Impact Decal Size Range"))
	FVector2D ImpactDecalSizeRange = FVector2D(3.0f, 7.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon|Gun|Impact", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float ImpactDecalDepth = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon|Gun|Impact")
	FRotator ImpactDecalRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon|Gun|Impact", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float ImpactDecalLifeSpan = 10.0f;

	bool HasAnyData() const
	{
		return bEnableAutomaticFire
			|| MuzzleFlashCueTag.IsValid()
			|| MuzzleSocketName != EWeaponSocketName::Muzzle
			|| !ImpactDecalMaterial.IsNull();
	}

	FName GetResolvedMuzzleSocketName() const
	{
		return PdWeaponSockets::ToName(MuzzleSocketName);
	}
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FWeaponAIDefinitionData
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon|AI|Ranged", meta = (ClampMin = "0.0", ForceUnits = "s", DisplayName = "AI Ranged Target Lock Delay", ToolTip = "Delay between the AI locking the target location and firing at that locked location. Moving players can dodge by leaving the locked position before the shot is released."))
	float RangedTargetLockDelay = 0.2f;

	bool HasAnyData() const
	{
		return !FMath::IsNearlyEqual(RangedTargetLockDelay, 0.2f);
	}
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FWeaponDefinitionData
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon")
	FWeaponEquipDefinitionData Equip;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon")
	FWeaponAttackDefinitionData Attack;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon")
	FWeaponMovementDefinitionData Movement;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon")
	FWeaponHitReactDefinitionData HitReact;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon")
	FWeaponAimDefinitionData Aim;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon")
	FBowWeaponDefinitionData Bow;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon")
	FGunWeaponDefinitionData Gun;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Weapon")
	FWeaponAIDefinitionData AI;

	bool HasAnyData() const
	{
		return Equip.HasAnyData()
			|| Attack.HasAnyData()
			|| Movement.HasAnyData()
			|| HitReact.HasAnyData()
			|| Aim.HasAnyData()
			|| Bow.HasAnyData()
			|| Gun.HasAnyData()
			|| AI.HasAnyData();
	}
};
