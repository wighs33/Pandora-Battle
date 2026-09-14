#pragma once

#include "CoreMinimal.h"
#include "SkillAuraSettings.generated.h"

class AEffectAreaBase;

/** 오라 액션의 장판 생성 설정. 회복 설정은 같은 액션의 Healing에서 읽는다. */
USTRUCT(BlueprintType)
struct LABPROJECT_API FAuraSkillConfig
{
	GENERATED_BODY()

	/** 오라 실행 중 장판 액터를 생성한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Aura|Effect Area")
	bool bSpawnEffectArea = false;

	/** 오라가 생성할 장판 액터 클래스. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Aura|Effect Area", meta = (EditCondition = "bSpawnEffectArea"))
	TSubclassOf<AEffectAreaBase> EffectAreaClass;

	/** 생성한 장판 액터의 수명(초). 0이면 수명을 별도로 지정하지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Aura|Effect Area", meta = (EditCondition = "bSpawnEffectArea", ClampMin = "0.0", ForceUnits = "s"))
	double EffectAreaLifeSpan = 10.0;

	/** 장판 생성 위치의 월드 좌표 보정. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Aura|Effect Area", meta = (EditCondition = "bSpawnEffectArea"))
	FVector EffectAreaSpawnOffset = FVector::ZeroVector;

	/** 지면과 겹치지 않도록 장판 위치에 더하는 높이(cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Aura|Ground Effects",
		meta = (ClampMin = "0.0", ForceUnits = "cm", DisplayName = "Ground Effect Z Offset"))
	double GroundEffectZOffset = 3.0;

	/** 오라 유지 중 장판을 주기적으로 생성한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Aura|Effect Area|Repeat", meta = (EditCondition = "bSpawnEffectArea"))
	bool bRepeatEffectAreaSpawn = false;

	/** 장판을 반복 생성하는 간격(초). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Aura|Effect Area|Repeat", meta = (EditCondition = "bSpawnEffectArea && bRepeatEffectAreaSpawn", ClampMin = "0.1", ForceUnits = "s"))
	double EffectAreaSpawnInterval = 2.0;

	/** 장판 효과 대상에서 시전자를 제외한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Aura|Effect Area|Filter", meta = (EditCondition = "bSpawnEffectArea"))
	bool bEffectAreaIgnoreSourceActor = false;

	/** 장판 효과를 적에게만 적용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Aura|Effect Area|Filter", meta = (EditCondition = "bSpawnEffectArea"))
	bool bEffectAreaAffectEnemiesOnly = false;
};
