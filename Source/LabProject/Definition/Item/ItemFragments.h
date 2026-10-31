#pragma once

#include "CoreMinimal.h"
#include "Common/CollisionChannels.h"
#include "Definition/Item/WeaponDefinitionData.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Templates/SubclassOf.h"
#include "ItemFragments.generated.h"

class AActor;
class UGameplayEffect;
class UMaterialInterface;

/**
 * 아이템 정의에 붙이는 데이터 조각의 베이스.
 *
 * 아이템마다 필요한 조각만 정의의 Fragments 배열에 담는다. 정의 애셋의 고정 데이터라 클라이언트도 같은 값을 읽으므로 복제하지 않는다.
 */
USTRUCT(BlueprintType, meta = (Hidden))
struct LABPROJECT_API FItemFragment
{
	GENERATED_BODY()
};

/** 장착하면 더해지는 능력치. 강화 보너스도 이 값을 기준으로 계산한다. */
USTRUCT(BlueprintType, meta = (DisplayName = "Stat"))
struct LABPROJECT_API FItemFragment_Stat : public FItemFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Stat")
	TMap<FGameplayTag, float> Map_Stat_Magnitude;
};

/** 퀵슬롯에서 써서 효과를 적용하는 소모품. */
USTRUCT(BlueprintType, meta = (DisplayName = "Consumable"))
struct LABPROJECT_API FItemFragment_Consumable : public FItemFragment
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	int32 GetSafeQuantityToConsume() const
	{
		return FMath::Max(1, QuantityToConsume);
	}

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Consumable")
	TSubclassOf<UGameplayEffect> ConsumeGameplayEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Consumable")
	TMap<FGameplayTag, float> Map_Consume_Magnitude;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Consumable", meta = (ClampMin = "1", UIMin = "1"))
	int32 QuantityToConsume = 1;
};

/** 보상 상자에서 나올 수 있는 아이템. 이 조각이 없으면 상자에서 나오지 않는다. */
USTRUCT(BlueprintType, meta = (DisplayName = "Drop"))
struct LABPROJECT_API FItemFragment_Drop : public FItemFragment
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	/** 상자가 아이템을 고를 때 쓰는 가중치. 0이면 고르지 않는다. */
	float GetDropWeight() const
	{
		return FMath::Max(0.0f, DropRate);
	}

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Drop", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float DropRate = 1.0f;
};

/** 장착해서 휘두르거나 쏘는 무기의 공통 데이터. 활과 총은 각자의 조각을 함께 붙인다. */
USTRUCT(BlueprintType, meta = (DisplayName = "Weapon"))
struct LABPROJECT_API FItemFragment_Weapon : public FItemFragment
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	float GetSafeAttackStaminaCost() const
	{
		return FMath::IsFinite(Attack.StaminaCost) ? FMath::Max(Attack.StaminaCost, 0.0f) : 0.0f;
	}

	/** 조준하는 무기는 이동 속도를 바꾸지 않는다. */
	float GetEquippedMovementSpeedMultiplier() const
	{
		if (Aim.bSupportsInput)
		{
			return 1.0f;
		}

		const float ConfiguredMultiplier = Movement.MeleeEquippedMovementSpeedMultiplier;
		return FMath::IsFinite(ConfiguredMultiplier) ? FMath::Max(ConfiguredMultiplier, 0.01f) : 1.0f;
	}

public:
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
	FWeaponAIDefinitionData AI;
};

/** 화살을 당겨 쏘는 활. Weapon 조각과 함께 붙인다. */
USTRUCT(BlueprintType, meta = (DisplayName = "Bow"))
struct LABPROJECT_API FItemFragment_Bow : public FItemFragment
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	FItemFragment_Bow()
	{
		TraceObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));
		TraceObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
		TraceObjectTypes.Add(UEngineTypes::ConvertToObjectType(CollisionChannels::HitableBody()));
	}

	FName GetResolvedArrowAttachSocketName() const
	{
		return WeaponSockets::ToName(ArrowAttachSocketName);
	}

public:
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
};

/** 히트스캔으로 쏘는 총. Weapon 조각과 함께 붙인다. */
USTRUCT(BlueprintType, meta = (DisplayName = "Gun"))
struct LABPROJECT_API FItemFragment_Gun : public FItemFragment
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	FItemFragment_Gun()
	{
		TraceObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));
		TraceObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
		TraceObjectTypes.Add(UEngineTypes::ConvertToObjectType(CollisionChannels::HitableBody()));
		ImpactDecalObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));
	}

	FName GetResolvedMuzzleSocketName() const
	{
		return WeaponSockets::ToName(MuzzleSocketName);
	}

public:
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
};
