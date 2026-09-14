#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SkillGameplayEffectConfig.generated.h"

class UGameplayEffect;

/** GameplayEffect 클래스와 SetByCaller 수치를 전달하는 공통 값 객체. */
USTRUCT(BlueprintType)
struct LABPROJECT_API FSkillGameplayEffectConfig
{
	GENERATED_BODY()

	/** 적용할 GameplayEffect 클래스. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Gameplay Effect")
	TSubclassOf<UGameplayEffect> GameplayEffectClass;

	/** Magnitude를 전달할 SetByCaller 태그. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Gameplay Effect", meta = (Categories = "Data"))
	FGameplayTag MagnitudeDataTag;

	/** GameplayEffect에 전달할 기본 수치. 피해로 사용할 때 공격력 보정이 적용된다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Gameplay Effect")
	double Magnitude = 0.0;
};
