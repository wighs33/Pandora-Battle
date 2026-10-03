#pragma once

#include "CoreMinimal.h"
#include "Common/CollisionChannels.h"
#include "Engine/EngineTypes.h"
#include "SkillActorFieldSettings.generated.h"

class AActor;
class UAnimMontage;
class UMaterialInterface;
class UMaterialParameterCollection;
class UNiagaraSystem;

/** 파워업 액터 전용 강화 연출 리소스. */
USTRUCT(BlueprintType)
struct LABPROJECT_API FSkillPowerUpPresentationSettings
{
	GENERATED_BODY()

public:
	/** 파워업 액터가 사용하는 강화 연출 몽타주. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Actor Field|Power Up Presentation")
	TObjectPtr<UAnimMontage> PowerUpMontage;

	/** 파워업 강화 연출의 캐릭터 오버레이. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Actor Field|Power Up Presentation")
	TObjectPtr<UMaterialInterface> OverlayMaterial;

	/** 파워업 강화 연출에서 캐릭터에 부착할 Niagara. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Actor Field|Power Up Presentation")
	TObjectPtr<UNiagaraSystem> AttachedNiagaraSystem;

	/** 파워업 강화 연출에서 조정할 머티리얼 파라미터 컬렉션. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Actor Field|Power Up Presentation")
	TObjectPtr<UMaterialParameterCollection> MaterialParameterCollection;

	// Public API ------------------------------------------------------------------------------------------------------
	bool IsComplete() const
	{
		return PowerUpMontage
			&& OverlayMaterial
			&& AttachedNiagaraSystem
			&& MaterialParameterCollection;
	}
};

/** Actor Field 액션이 배치하는 액터의 생성과 트리거 수명. 블랙홀·파워업 액터 전용 옵션을 함께 제공한다. */
USTRUCT(BlueprintType)
struct LABPROJECT_API FSkillActorFieldSettings
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	FSkillActorFieldSettings()
		: GroundTraceChannel(LabCollisionChannels::VisibilityTrace())
	{
		SpawnSocketNames.SetNum(6);
	}

public:
	/** 지정된 소켓 목록을 사용해 배치 액터를 생성한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Actor Field|Spawn")
	bool bUseSpawnSockets = true;

	/** 생성한 배치 액터를 해당 소켓에 부착한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Actor Field|Spawn", meta = (EditCondition = "bUseSpawnSockets", DisplayName = "Attach Spawned Actor To Socket"))
	bool bAttachSpawnedActorToSocket = true;

	/** 배치 액터를 생성할 최대 6개의 소켓. 비어 있는 슬롯은 건너뛴다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Actor Field", meta = (EditFixedSize))
	TArray<FName> SpawnSocketNames;

	/** 소켓별 배치 액터 생성 간격(초). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Actor Field", meta = (ClampMin = "0.0", ForceUnits = "s"))
	double SpawnInterval = 0.0;

	/** 설정된 액터 생성 순서를 반복한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Actor Field|Repeat")
	bool bRepeatSpawnSequence = false;

	/** 액터 생성 순서를 다시 시작하는 간격(초). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Actor Field|Repeat", meta = (EditCondition = "bRepeatSpawnSequence", ClampMin = "0.1", ForceUnits = "s"))
	double RepeatSpawnInterval = 2.0;

	/** 배치할 액터 클래스. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Actor Field")
	TSubclassOf<AActor> FieldActorClass;

	/** 파워업 액터에서만 사용하는 강화 연출 리소스. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Actor Field", meta = (ShowOnlyInnerProperties))
	FSkillPowerUpPresentationSettings PowerUpPresentation;

	/** 생성 기준 좌표계에서의 위치 보정. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Actor Field|Spawn")
	FVector SpawnLocationOffset = FVector::ZeroVector;

	/** 생성 기준 회전에 더할 회전 보정. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Actor Field|Spawn")
	FRotator SpawnRotationOffset = FRotator::ZeroRotator;

	/** 생성 위치에서 지면을 검사해 액터를 지면 위에 배치한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Actor Field|Spawn")
	bool bProjectSpawnToGround = false;

	/** 생성 위치의 지면을 찾는 트레이스 채널. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Actor Field|Spawn", meta = (EditCondition = "bProjectSpawnToGround"))
	TEnumAsByte<ETraceTypeQuery> GroundTraceChannel;

	/** 생성 위치보다 위에서 지면 검사를 시작할 높이(cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Actor Field|Spawn", meta = (EditCondition = "bProjectSpawnToGround", ClampMin = "0.0", ForceUnits = "cm"))
	double GroundTraceStartHeight = 100.0;

	/** 아래쪽 지면을 검사할 거리(cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Actor Field|Spawn", meta = (EditCondition = "bProjectSpawnToGround", ClampMin = "0.0", ForceUnits = "cm"))
	double GroundTraceDepth = 10000.0;

	/** 액터 생성 위치가 다른 물체와 겹칠 때의 처리 방식. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Actor Field|Spawn")
	ESpawnActorCollisionHandlingMethod SpawnCollisionHandling = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	/** 생성한 액터의 네트워크 복제를 활성화한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Actor Field|Actor")
	bool bForceReplicateSpawnedActor = true;

	/** 클라이언트에 복제될 시간을 확보하는 최소 액터 수명(초). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Actor Field|Actor|Network", meta = (EditCondition = "bForceReplicateSpawnedActor", ClampMin = "0.0", ForceUnits = "s"))
	double MinimumReplicatedActorLifetime = 1.0;

	/** 배치 액터에 명시적인 수명을 적용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Actor Field|Actor", meta = (DisplayName = "Use Spawned Actor Life Span"))
	bool bUseSpawnedActorLifeSpan = false;

	/** 명시적으로 지정할 배치 액터의 수명(초). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Actor Field|Actor", meta = (EditCondition = "bUseSpawnedActorLifeSpan", EditConditionHides, ClampMin = "0.0", ForceUnits = "s"))
	double SpawnedActorLifeSpan = 2.0;

	/** Ability 종료 시 생성한 배치 액터를 제거한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Actor Field|Actor")
	bool bDestroySpawnedActorsOnAbilityEnd = false;

	/** 피해 Overlap 이벤트를 받을 액터의 충돌 컴포넌트 이름. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Actor Field|Trigger")
	FName TriggerComponentName = TEXT("Box");

	/** 생성 순간 이미 트리거 안에 있는 대상에게도 피해를 적용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Actor Field|Trigger")
	bool bDamageExistingOverlapsOnSpawn = true;

	/** 마지막 액터 생성 이후 피해 트리거를 유지할 시간(초). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Actor Field|Trigger", meta = (ClampMin = "0.0", ForceUnits = "s"))
	double TriggerActiveDurationAfterLastSpawn = 0.5;

	/** 배치 액터의 직접 트리거 피해에서 시전자를 제외한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Actor Field|Damage")
	bool bIgnoreSourceActor = true;

	/** 블랙홀 액터가 스킬 유지 시간 동안 적을 끌어당긴다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Actor Field|Black Hole|Pull", meta = (DisplayName = "Pull Enemies For Entire Skill Duration"))
	bool bBlackHolePullEnemiesDuringGrowth = true;

	/** 블랙홀과 지면 연출이 시작 크기에서 최종 크기로 자라는 시간(초). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Actor Field|Black Hole|Growth",
		meta = (DisplayName = "Growth Duration", ToolTip = "Time in seconds for the Black Hole and floor effects to grow from their start scale to their end scale.", ClampMin = "0.01", UIMin = "0.01", ForceUnits = "s"))
	double BlackHoleGrowthDuration = 3.0;

	/** 블랙홀이 적을 끌어당기는 범위(cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Actor Field|Black Hole|Pull", meta = (EditCondition = "bBlackHolePullEnemiesDuringGrowth", ClampMin = "0.0", ForceUnits = "cm"))
	double BlackHolePullRadius = 1200.0;

	/** 블랙홀 방향으로 추가하는 이동 속도(cm/s). 대상의 이동과 합산된다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Actor Field|Black Hole|Pull", meta = (EditCondition = "bBlackHolePullEnemiesDuringGrowth", ClampMin = "0.0", ForceUnits = "cm/s", DisplayName = "Pull Speed (Additive)", ToolTip = "Velocity added toward the Black Hole. Opposite movement remains active, so the effective pull is Pull Speed minus the target's movement speed."))
	double BlackHolePullSpeed = 650.0;

	/** 블랙홀 종료 시 공통 Damage로 범위 피해를 적용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Actor Field|Black Hole|Finish Damage")
	bool bBlackHoleApplyFinishAreaDamage = false;

	/** 블랙홀 종료 피해의 반경(cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Actor Field|Black Hole|Finish Damage", meta = (EditCondition = "bBlackHoleApplyFinishAreaDamage", ClampMin = "0.0", ForceUnits = "cm"))
	double BlackHoleFinishDamageRadius = 1200.0;

	/** 장판 효과 대상에서 시전자를 제외한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Actor Field|Effect Area")
	bool bEffectAreaIgnoreSourceActor = false;

	/** 장판 효과를 적에게만 적용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Actor Field|Effect Area")
	bool bEffectAreaAffectEnemiesOnly = false;
};
