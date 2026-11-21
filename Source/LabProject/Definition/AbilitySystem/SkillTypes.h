#pragma once

#include "CoreMinimal.h"
#include "SkillTypes.generated.h"

namespace SkillDebug
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

/**
 * 봇이 이 스킬을 쓴 뒤 스킬 지속 시간 동안 할 일. 사람은 스킬마다 알맞게 움직이지만 봇은 스스로 알 수 없어 스킬 데이터로 정한다.
 * 지속 시간이 짧은 스킬도 동작이 보이도록 최소 시간(BotSkillRules)만큼은 지킨다.
 */
UENUM(BlueprintType)
enum class ESkillBotFollowUp : uint8
{
	/** 쓰고 나면 평소처럼 싸운다. */
	None UMETA(DisplayName = "None"),
	/** 대상 둘레를 뛰어다닌다. 몸에 붙어 거듭 깔리는 장판(버닝 플레어)이 한곳에 쌓이지 않고 바닥에 고루 깔리게 한다. */
	RoamAroundTarget UMETA(DisplayName = "Roam Around Target"),
	/** 제자리에 서 있는다. 스킬이 끝날 때까지 움직이지 않는 스킬(블랙홀)에 쓴다. */
	HoldPosition UMETA(DisplayName = "Hold Position"),
	/** 다른 스킬 없이 기본 공격만 한다. 공격력을 올리는 스킬(다크 스테인)에 쓴다. */
	WeaponAttacksOnly UMETA(DisplayName = "Weapon Attacks Only")
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
