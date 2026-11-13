#pragma once

#include "Definition/Item/WeaponDefinitionData.h"
#include "CoreMinimal.h"
#include "Definition/Common/CombatSettings.h"
#include "Engine/DataAsset.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"
#include "UObject/PrimaryAssetId.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "PlayerPawnDefinition.generated.h"

class UAnimMontage;
class UGameplayEffect;
class UMaterialParameterCollection;
class UNiagaraSystem;

USTRUCT(BlueprintType)
struct LABPROJECT_API FPlayerInteractionSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Player|Interaction",
		meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float ServerValidationDistance = 250.0f;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FPlayerCameraPresentationSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Player|Camera|Occlusion")
	FName OcclusionEnabledParameterName = TEXT("OcclusionEnabled");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Player|Camera|Occlusion",
		meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float OcclusionDisableDistance = 50.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Player|Camera|Occlusion",
		meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float OcclusionReenableDistance = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Player|Camera|Occlusion")
	TArray<TEnumAsByte<ECollisionChannel>> OcclusionSurfaceObjectTypes;

	/**
	 * 화면 가운데를 가리는 면을 지우는 머티리얼 함수(MF_OcclusionFadeMask)에 플레이어 위치를 넘기는 컬렉션.
	 * 플레이어를 가릴 수 없는 면, 곧 플레이어보다 멀거나 발보다 낮은 면(내려다볼 때의 바닥)은 지우지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Player|Camera|Occlusion")
	TObjectPtr<UMaterialParameterCollection> OcclusionParameterCollection;

	/** 컬렉션 안의 거리 파라미터(cm): 카메라에서 플레이어까지. 이보다 먼 면은 지우지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Player|Camera|Occlusion")
	FName OcclusionDistanceParameterName = TEXT("OcclusionDistance");

	/** 컬렉션 안의 높이 파라미터(월드 Z, cm): 플레이어 발 높이 + OcclusionFloorClearance. 이보다 낮은 면은 지우지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Player|Camera|Occlusion")
	FName OcclusionFloorHeightParameterName = TEXT("OcclusionFloorHeight");

	/** 발밑 바닥과 낮은 턱이 지워지지 않게 발 높이에서 이만큼 위부터 지운다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Player|Camera|Occlusion",
		meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float OcclusionFloorClearance = 20.0f;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FPlayerAimSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Player|Aim|Network",
		meta = (ClampMin = "0.05", ForceUnits = "s"))
	float ReplicationInterval = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Player|Aim|Movement")
	FRotator DefaultRotationRate = FRotator(0.0f, 500.0f, 0.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Player|Aim|Movement")
	FRotator AimingRotationRate = FRotator(0.0f, 3000.0f, 0.0f);
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FPlayerActionPolicySettings
{
	GENERATED_BODY()

	/** 이동이 피격 반응을 끊을 때 이 태그 중 하나라도 가진 능력은 취소될 수 있다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Player|Action",
		meta = (Categories = "Action,GameplayAbility"))
	FGameplayTagContainer MovementHitReactCancelTags;
};

/**
 * APdPlayer의 데이터 기반 구성 설정을 제공한다.
 *
 * 각 컴포넌트는 자신에게 필요한 설정 묶음을 사용하며, APdPlayer는 기존 블루프린트와
 * 네이티브 호출부의 호환 진입점으로 유지한다.
 */
UCLASS(BlueprintType, Const)
class LABPROJECT_API UPlayerPawnDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void PostLoad() override;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	// Public API ------------------------------------------------------------------------------------------------------
	UPlayerPawnDefinition();
	static FPrimaryAssetId GetDefaultPrimaryAssetId();
	static FSoftObjectPath GetDefaultDefinitionPath();

	const FPlayerInteractionSettings& GetInteractionSettings() const { return Interaction; }
	const FPlayerCameraPresentationSettings& GetCameraSettings() const { return Camera; }
	const FPlayerAimSettings& GetAimSettings() const { return Aim; }
	const FPlayerActionPolicySettings& GetActionPolicySettings() const { return ActionPolicy; }
	const FCombatDamageSettings& GetCombatDamageSettings() const { return CombatDamageSettings; }
	const FUnarmedCombatSettings& GetUnarmedCombatSettings() const { return UnarmedCombatSettings; }

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Player|Interaction",
		meta = (AllowPrivateAccess = "true"))
	FPlayerInteractionSettings Interaction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Player|Camera",
		meta = (AllowPrivateAccess = "true"))
	FPlayerCameraPresentationSettings Camera;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Player|Aim",
		meta = (AllowPrivateAccess = "true"))
	FPlayerAimSettings Aim;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Player|Action",
		meta = (AllowPrivateAccess = "true"))
	FPlayerActionPolicySettings ActionPolicy;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Player|Combat|Unarmed",
		meta = (AllowPrivateAccess = "true"))
	FUnarmedCombatSettings UnarmedCombatSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Player|Combat|Damage", meta = (AllowPrivateAccess = "true"))
	FCombatDamageSettings CombatDamageSettings;
};
