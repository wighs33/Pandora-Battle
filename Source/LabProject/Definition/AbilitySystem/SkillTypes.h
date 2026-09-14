#pragma once

#include "CoreMinimal.h"
#include "SkillTypes.generated.h"

namespace LabSkillDebug
{
	/** 개발 빌드에서 lab.Skill.DebugDraw가 활성화된 경우에만 디버그 그리기를 허용한다. */
	LABPROJECT_API bool IsDrawingEnabled();
}

/** 스킬의 기능 분류가 아닌 실행 종료 정책. 여러 기능을 함께 사용하는 스킬에도 적용한다. */
UENUM(BlueprintType)
enum class ESkillType : uint8
{
	/** 실행할 동작이 완료되면 종료한다. */
	Instant UMETA(DisplayName = "Instant"),
	/** 입력을 유지하는 동안 실행하고 입력을 놓으면 종료한다. */
	Press UMETA(DisplayName = "Press"),
	/** 유지 액션이 Time.Duration을 사용하며, 액션 트리 완료 시 종료한다. */
	Duration UMETA(DisplayName = "Duration")
};

/** 스킬의 시간 정책. 기능 종류와 무관하게 공유한다. */
USTRUCT(BlueprintType)
struct LABPROJECT_API FSkillTimeSettings
{
	GENERATED_BODY()

	/** 스킬을 다시 사용할 수 있을 때까지의 대기시간(초). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Time", meta = (ClampMin = "0.0", ForceUnits = "s"))
	double CooldownDuration = 0.0;

	/** 오라·검기·소환·배치 등 유지 액션의 지속시간(초). 각 액션의 시작 시점을 기준으로 사용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Time", meta = (ClampMin = "0.0", ForceUnits = "s"))
	double Duration = 0.0;
};
