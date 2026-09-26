#pragma once

#include "CoreMinimal.h"
#include "Engine/CollisionProfile.h"
#include "GameplayTagContainer.h"
#include "SkillProjectileSettings.generated.h"

class AGameplayAbilityTargetActor;
class ASkillProjectile;
class UGameplayEffect;
class UMaterialInterface;
class UNiagaraSystem;

/** 투사체 Ability의 발사 확정 방식. 스킬의 기능 종류를 제한하지 않는다. */
UENUM(BlueprintType)
enum class EProjectileFireMode : uint8
{
	/** 발사 조건을 충족하면 별도 확정 입력 없이 발사한다. */
	Immediate UMETA(DisplayName = "Immediate"),
	/** 발사 준비와 조준을 거친 뒤 별도 확정 입력으로 발사한다. */
	HoldThenConfirm UMETA(DisplayName = "Hold Then Confirm")
};

/** 투사체 Ability의 발사, 비행, 조준 및 충돌 설정. */
USTRUCT(BlueprintType)
struct LABPROJECT_API FSkillProjectileSettings
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	FSkillProjectileSettings()
	{
		ProjectileSocketNames.SetNum(6);
	}

public:
	/** 투사체를 발사할 최대 6개의 소켓. 비어 있는 슬롯은 건너뛴다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile", meta = (EditFixedSize))
	TArray<FName> ProjectileSocketNames;

	/** 소켓별 투사체 발사 간격(초). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile", meta = (ClampMin = "0.0", ForceUnits = "s", DisplayName = "Socket Fire Interval"))
	double ProjectileSocketFireInterval = 0.0;

	/** 즉시 발사하거나, 준비·조준 후 별도 확정 입력으로 발사한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile")
	EProjectileFireMode FireMode = EProjectileFireMode::Immediate;

	/** 몽타주에서 발사 시점을 알리는 GameplayEvent 태그. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile", meta = (Categories = "Event"))
	FGameplayTag FireEventTag;

	/** 발사 준비 중 투사체를 시작 크기에서 최종 크기로 확대한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Scale")
	bool bGrowProjectileSize = false;

	/** 확대 시작 시 투사체의 크기 배율. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Scale", meta = (EditCondition = "bGrowProjectileSize"))
	FVector ProjectileStartScale = FVector::OneVector;

	/** 확대 완료 시 투사체의 크기 배율. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Scale", meta = (EditCondition = "bGrowProjectileSize"))
	FVector ProjectileFinalScale = FVector::OneVector;

	/** 투사체 크기 변화에 걸리는 시간(초). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Scale", meta = (EditCondition = "bGrowProjectileSize", ClampMin = "0.0", ForceUnits = "s"))
	double ProjectileScaleDuration = 0.0;

	/** 크기 변화와 함께 갱신할 Niagara Vector2D 파라미터 이름. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Scale", meta = (EditCondition = "bGrowProjectileSize"))
	FName GrowthUserParameterName = NAME_None;

	/** 크기 변화 시작 시 Niagara 파라미터 값. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Scale", meta = (EditCondition = "bGrowProjectileSize"))
	FVector2D GrowthUserParameterStartValue = FVector2D::UnitVector;

	/** 크기 변화 완료 시 Niagara 파라미터 값. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Scale", meta = (EditCondition = "bGrowProjectileSize"))
	FVector2D GrowthUserParameterFinalValue = FVector2D::UnitVector;

	/** 생성하고 발사할 투사체 액터 클래스. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile")
	TSubclassOf<ASkillProjectile> ProjectileActorClass;

	/** 투사체의 이동 속도(cm/s). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	double ProjectileSpeed = 0.0;

	/** 투사체 충돌 구체의 반경(cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	double ProjectileRadius = 50.0;

	/** 목표 위치를 향한 곡사 궤적을 사용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Trajectory")
	bool bUseArcTrajectory = false;

	/** 곡사 궤적의 높이 보정(cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Trajectory", meta = (EditCondition = "bUseArcTrajectory", ClampMin = "0.0", ForceUnits = "cm"))
	double ProjectileArcHeight = 0.0;

	/** 곡사 궤적에 적용할 중력 배율. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Trajectory", meta = (EditCondition = "bUseArcTrajectory", ClampMin = "0.0"))
	double ProjectileArcGravityScale = 1.0;

	/** 충돌 직후 투사체를 제거하지 않고 충돌 지점에 남긴다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Impact")
	bool bStickOnImpact = false;

	/** 충돌 지점에 남긴 투사체의 수명(초). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Impact", meta = (EditCondition = "bStickOnImpact", EditConditionHides, ClampMin = "0.01", ForceUnits = "s"))
	double PostImpactLifeSpan = 1.0;

	/** 생성 기준 좌표계에서의 위치 보정. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Spawn")
	FVector SpawnLocationOffset = FVector::ZeroVector;

	/** 시전자와 겹치지 않도록 확보할 최소 전방 생성 거리(cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Spawn", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	double MinimumForwardSpawnOffset = 0.0;

	/** 카메라 조준 트레이스의 최대 거리(cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Targeting", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	double TargetTraceMaxRange = 0.0;

	/** 카메라 조준 트레이스에 사용할 충돌 프로필. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Targeting")
	FCollisionProfileName TargetTraceProfile = FCollisionProfileName(TEXT("NoCollision"));

	/** 투사체 생성 위치에서 확보할 최소 목표 거리(cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Targeting", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	double MinimumTargetDistanceFromSpawn = 1000.0;

	/** 조준 결과를 투사체 발사 방향의 상하 각도에 반영한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Targeting")
	bool bTraceAffectsAimPitch = true;

	/** 개발용 스킬 디버그 CVar가 켜진 경우 조준 트레이스를 그린다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Debug")
	bool bDrawTargetTraceDebug = false;

	/** 충돌 연출의 지면 검사 시작 높이(cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Targeting", meta = (ClampMin = "0.0", ForceUnits = "cm", DisplayName = "Ground Trace Start Height"))
	double TargetGroundTraceStartHeight = 500.0;

	/** 충돌 연출의 지면 검사 깊이(cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Targeting", meta = (ClampMin = "100.0", ForceUnits = "cm", DisplayName = "Ground Trace Depth"))
	double TargetGroundTraceDepth = 100000.0;

	/** 투사체 발사 위치에 재생할 Niagara. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Niagara")
	TObjectPtr<UNiagaraSystem> MuzzleNiagaraSystem;

	/** 비행 중인 투사체에 적용할 Niagara. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Niagara")
	TObjectPtr<UNiagaraSystem> ProjectileNiagaraSystem;

	/** 투사체 충돌 시 재생할 Niagara. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Niagara")
	TObjectPtr<UNiagaraSystem> HitNiagaraSystem;

	/** 충돌 연출을 충돌 위치 아래의 지면에 배치한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Niagara")
	bool bSpawnHitNiagaraOnGround = false;

	/** GAS TargetActor로 지면 목표 위치를 선택한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Ground Targeting")
	bool bUseGroundTargeting = false;

	/** 투사체의 지면 목표 선택을 담당할 TargetActor 클래스. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Ground Targeting", meta = (EditCondition = "bUseGroundTargeting"))
	TSubclassOf<AGameplayAbilityTargetActor> GroundTargetActorClass;

	/** 지면 조준의 최대 거리(cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Ground Targeting", meta = (EditCondition = "bUseGroundTargeting", ClampMin = "0.0", ForceUnits = "cm"))
	double GroundTargetingMaxRange = 3000.0;

	/** 지면 조준에 사용할 충돌 프로필. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Ground Targeting", meta = (EditCondition = "bUseGroundTargeting"))
	FCollisionProfileName GroundTargetingTraceProfile = FCollisionProfileName(TEXT("BlockAll"));

	/** 지면 TargetActor의 충돌 검사 반경(cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Ground Targeting", meta = (EditCondition = "bUseGroundTargeting", ClampMin = "0.0", ForceUnits = "cm"))
	double GroundTargetingCollisionRadius = 0.0;

	/** 지면 TargetActor의 충돌 검사 높이(cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Ground Targeting", meta = (EditCondition = "bUseGroundTargeting", ClampMin = "0.0", ForceUnits = "cm"))
	double GroundTargetingCollisionHeight = 0.0;

	/** 지면 조준 결과를 상하 조준 각도에 반영한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Ground Targeting", meta = (EditCondition = "bUseGroundTargeting"))
	bool bGroundTargetingTraceAffectsAimPitch = true;

	/** 개발용 스킬 디버그 CVar가 켜진 경우 지면 조준 정보를 그린다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Debug", meta = (EditCondition = "bUseGroundTargeting"))
	bool bDrawGroundTargetingDebug = false;

	/** 투사체 목표 위치에 표시할 데칼 머티리얼. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Decal")
	TObjectPtr<UMaterialInterface> TargetDecal;

	/** 투사체 목표 데칼의 시작 크기(cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Decal", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	double TargetDecalSize = 512.0;

	/** 발사 준비 중 목표 데칼을 최종 크기까지 확대한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Decal")
	bool bGrowTargetDecalSize = false;

	/** 확대 완료 시 투사체 목표 데칼 크기(cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Decal", meta = (EditCondition = "bGrowTargetDecalSize", ClampMin = "0.0", ForceUnits = "cm"))
	double TargetDecalFinalSize = 0.0;

	/** 지면 조준 데칼에 전달할 색상. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Projectile|Decal", meta = (EditCondition = "bUseGroundTargeting"))
	FLinearColor TargetDecalColor = FLinearColor(0.1f, 0.1f, 0.1f, 1.0f);

	/** 공통 상태 효과 정의가 없을 때 투사체 적중에 적용할 GameplayEffect. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Gameplay Effect|Status")
	TSubclassOf<UGameplayEffect> StatusEffectClass;

	/** 상태 효과 GameplayEffect Spec을 생성할 때 사용하는 레벨. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Gameplay Effect|Status", meta = (ClampMin = "1.0"))
	float StatusEffectLevel = 1.0f;
};
