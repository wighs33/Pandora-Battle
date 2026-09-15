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
	/** 스킬 활성화부터 Time.Duration 동안 유지하고, 종료 시 실행 중인 모든 액션을 정리한다. */
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

	/** Duration 스킬의 전체 지속시간(초). 조준·준비·몽타주를 포함하며, 액션이 늦게 시작해도 종료 시점은 늘어나지 않는다. 0이면 액션 없이 종료한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Time", meta = (ClampMin = "0.0", ForceUnits = "s"))
	double Duration = 0.0;
};
