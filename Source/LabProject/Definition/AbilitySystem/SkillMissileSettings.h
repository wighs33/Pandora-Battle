#pragma once

#include "CoreMinimal.h"
#include "SkillMissileSettings.generated.h"

class AGameplayAbilityTargetActor;

/** Niagara 미사일의 조준과 반복 피해 정책. 연출·몽타주·피해량은 공통 설정에서 읽는다. */
USTRUCT(BlueprintType)
struct LABPROJECT_API FMissileSkillConfig
{
	GENERATED_BODY()

	/** 미사일 Niagara에 목표 위치를 전달하는 벡터 파라미터 이름. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Missile|Niagara")
	FName AimPositionParameterName = TEXT("aim_position");

	/** 플레이어의 공격 위치 선택을 처리하는 GAS TargetActor 클래스. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Missile|Targeting")
	TSubclassOf<AGameplayAbilityTargetActor> TargetActorClass;

	/** 미사일의 자동 표적 탐색 반경(cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Missile|Targeting", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	double AutoTargetSearchRadius = 3000.0;

	/** 자동 표적 탐색 중심을 시전자 전방으로 옮길 거리(cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Missile|Targeting", meta = (ForceUnits = "cm"))
	double AutoTargetForwardOffset = 0.0;

	/** 조준 위치와 표적 검증에 사용하는 최대 거리(cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Missile|Targeting", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	double TargetingMaxRange = 3000.0;

	/** 조준 트레이스에 사용할 충돌 프로필 이름. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Missile|Targeting")
	FName TargetingTraceProfileName = TEXT("BlockAll");

	/** 지면 TargetActor의 충돌 검사 반경(cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Missile|Targeting", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	double TargetingCollisionRadius = 0.0;

	/** 지면 TargetActor의 충돌 검사 높이(cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Missile|Targeting", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	double TargetingCollisionHeight = 0.0;

	/** 조준 트레이스 결과를 발사 방향의 상하 각도에 반영한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Missile|Targeting")
	bool bTargetingTraceAffectsAimPitch = true;

	/** 개발용 스킬 디버그 CVar가 켜진 경우 조준 정보를 그린다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Missile|Targeting")
	bool bDebugTargeting = false;

	/** 미사일이 목표 캐릭터에서 추적할 소켓 이름. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Missile|Targeting")
	FName TargetSocketName = NAME_None;

	/** 미사일 발사 후 첫 피해를 시작하기까지의 대기시간(초). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Missile|Damage Over Time", meta = (ClampMin = "0.0", ForceUnits = "s"))
	double DamageStartDelay = 1.0;

	/** 미사일의 반복 피해를 적용하는 기간(초). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Missile|Damage Over Time", meta = (ClampMin = "0.0", ForceUnits = "s", DisplayName = "Damage Application Duration"))
	double DamageApplicationDuration = 3.0;

	/** 미사일의 반복 피해 간격(초). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Missile|Damage Over Time", meta = (ClampMin = "0.05", ForceUnits = "s"))
	double DamageInterval = 0.5;

	/** 미사일 목표 위치를 중심으로 피해를 적용할 반경(cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Missile|Damage Over Time", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	double DamageRadius = 256.0;

};
