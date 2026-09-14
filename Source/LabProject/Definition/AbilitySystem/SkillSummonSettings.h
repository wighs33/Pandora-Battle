#pragma once

#include "CoreMinimal.h"
#include "Common/CollisionChannels.h"
#include "Engine/EngineTypes.h"
#include "SkillSummonSettings.generated.h"

class AActor;

/** 소환 액터의 생성, 상승, Niagara 활성화와 피해 시작 시점. */
USTRUCT(BlueprintType)
struct LABPROJECT_API FSkillSummonSettings
{
	GENERATED_BODY()

	FSkillSummonSettings()
		: GroundTraceChannel(LabCollisionChannels::VisibilityTrace())
	{
	}

	// 액터와 수명

	/** 생성할 소환 액터 클래스. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon")
	TSubclassOf<AActor> SummonedActorClass;

	/** 시간 유지 정책이 없는 소환 Ability의 대체 유지 시간(초). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon|Actor", meta = (ClampMin = "0.0", ForceUnits = "s"))
	double SummonedActorLifeSpan = 0.0;

	/** 생성한 액터의 네트워크 복제를 활성화한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon|Actor")
	bool bForceReplicateSpawnedActor = true;

	/** 클라이언트에 복제될 시간을 확보하는 최소 액터 수명(초). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon|Network", meta = (ClampMin = "0.0", ForceUnits = "s"))
	double MinimumReplicatedActorLifetime = 0.5;

	// 생성 위치와 충돌

	/** 소환 위치의 기준으로 사용할 캐릭터 소켓 이름. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon")
	FName SpawnSocketName = NAME_None;

	/** 소환 기준 위치에서 전방으로 띄울 거리(cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	double SpawnForwardDistance = 300.0;

	/** 생성 기준 좌표계에서의 위치 보정. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon")
	FVector SpawnLocationOffset = FVector::ZeroVector;

	/** 생성 기준 회전에 더할 회전 보정. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon")
	FRotator SpawnRotationOffset = FRotator::ZeroRotator;

	/** 소환 기준 회전에서 시전자의 수평 회전만 사용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon|Spawn")
	bool bUseOwnerYawOnly = true;

	/** 액터 생성 위치가 다른 물체와 겹칠 때의 처리 방식. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon|Spawn")
	ESpawnActorCollisionHandlingMethod SpawnCollisionHandling = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// 지면 배치

	/** 생성 위치에서 지면을 검사해 액터를 지면 위에 배치한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon")
	bool bProjectSpawnToGround = true;

	/** 생성 위치의 지면을 찾는 트레이스 채널. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon|Ground", meta = (EditCondition = "bProjectSpawnToGround"))
	TEnumAsByte<ETraceTypeQuery> GroundTraceChannel;

	/** 생성 위치보다 위에서 지면 검사를 시작할 높이(cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon|Ground", meta = (EditCondition = "bProjectSpawnToGround", ClampMin = "0.0", ForceUnits = "cm"))
	double GroundTraceStartHeight = 500.0;

	/** 아래쪽 지면을 검사할 거리(cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon|Ground", meta = (EditCondition = "bProjectSpawnToGround", ClampMin = "0.0", ForceUnits = "cm"))
	double GroundTraceDepth = 2000.0;

	// 상승 동작

	/** 소환 액터를 지면 아래에서 생성한 뒤 최종 위치로 상승시킨다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon|Rise")
	bool bRiseFromUnderground = false;

	/** 소환 시작 위치를 지면 아래로 내릴 거리(cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon|Rise", meta = (EditCondition = "bRiseFromUnderground", ClampMin = "0.0", ForceUnits = "cm"))
	double RiseDistanceBelowGround = 250.0;

	/** 소환 액터가 상승하는 속도(cm/s). 거리와 속도로 상승 시간을 계산한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon|Rise", meta = (EditCondition = "bRiseFromUnderground", ClampMin = "0.0", ForceUnits = "cm/s"))
	double RiseSpeed = 250.0;

	/** 소환 액터의 상승 위치를 갱신하는 타이머 간격(초). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon|Rise", meta = (ClampMin = "0.005", ForceUnits = "s"))
	double RiseTickInterval = 0.02;

	// 상승 이후의 연출과 피해

	/** 상승 완료 후 활성화할 소환 액터의 Niagara 컴포넌트 이름 또는 태그. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon|Laser")
	FName LaserNiagaraComponentName = TEXT("LaserNiagara");

	/** Niagara 컴포넌트 이름이 없으면 액터의 모든 Niagara를 제어한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon|Laser")
	bool bActivateAllNiagaraComponentsWhenNameNone = true;

	/** 생성 직후 레이저 Niagara를 끄고 상승 완료까지 기다린다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon|Laser")
	bool bDeactivateLaserNiagaraOnSpawn = true;

	/** 레이저 Niagara 활성화 시 시뮬레이션을 처음부터 재생한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon|Laser")
	bool bResetLaserNiagaraOnActivate = true;

	/** 소환 액터가 상승을 마친 뒤 피해 트리거를 켜기까지의 대기시간(초). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Summon|Damage", meta = (ClampMin = "0.0", ForceUnits = "s"))
	double TriggerDamageDelay = 0.0;

	float GetRiseDistance() const
	{
		return bRiseFromUnderground ? static_cast<float>(FMath::Max(RiseDistanceBelowGround, 0.0)) : 0.0f;
	}

	float GetRiseDuration() const
	{
		return RiseSpeed > 0.0 ? static_cast<float>(RiseDistanceBelowGround / RiseSpeed) : 0.0f;
	}
};
