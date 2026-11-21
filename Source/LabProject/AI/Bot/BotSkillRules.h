#pragma once

#include "CoreMinimal.h"

class AActor;
class APawn;
class UPandoraDefinition;
class USkillDefinition;
class UWorld;

/**
 * 봇이 판도라 스킬을 쓰는 공통 규칙. 훈련장 봇과 대기실에서 넣은 봇이 함께 쓴다.
 * 사람처럼 보이도록 정한 규칙이다(트레일러 촬영 때 사용자와 정한 규칙, 2026-11-13):
 * 멈춰 선 뒤에 공격·시전하고, 스킬 사이에 숨을 돌리며, 스킬마다 정한 뒤처리(ESkillBotFollowUp)를 지킨다.
 */
namespace BotSkillRules
{
	/** 공격·시전을 시작해도 되는 수평 속도. 달리던 힘으로 미끄러지며 휘두르지 않게 이보다 느려진 뒤에 시작한다. */
	constexpr float SettledSpeed = 90.0f;

	/** 시전 동작이 이보다 짧아도 이만큼은 시전 중으로 본다. 몽타주가 없는 스킬도 쓰는 모습이 보이게 한다. */
	constexpr float MinimumCastSeconds = 0.35f;

	/** 시전 동작이 끝나지 않아도 이 시간이 지나면 시전을 끝난 것으로 본다(끊긴 스킬에 묶이지 않게). */
	constexpr float MaximumCastSeconds = 8.0f;

	/** 멈춰 서서 대상을 보는 데 이보다 오래 걸리면 이번 시전은 그만둔다. */
	constexpr float MaximumPrepareSeconds = 1.5f;

	/** 시전하려면 대상과의 방향 차이가 이 각도 안이어야 한다. */
	constexpr float CastFacingToleranceDegrees = 15.0f;

	/** 시전하려고 대상 쪽으로 도는 빠르기. */
	constexpr float CastTurnRateDegrees = 720.0f;

	/** 스킬을 쓰지 못했을 때(쿨다운·거리) 다시 살피기까지의 시간. */
	constexpr float RetryDelaySeconds = 0.5f;

	/** 봇이 쓸 수 있는 판도라인지. 모든 스킬 칸에 실행 액션이 있는 스킬이 있어야 한다(스킬이 비어 있는 판도라는 아직 만드는 중이다). */
	LABPROJECT_API bool IsPandoraUsableByBots(const UPandoraDefinition& Pandora);

	/** 후보에서 겹치지 않게 Count개를 무작위로 고른다. 후보가 적으면 있는 만큼 고른다. */
	LABPROJECT_API TArray<const UPandoraDefinition*> PickRandomPandoras(TArray<const UPandoraDefinition*> Candidates, int32 Count);

	/** 스킬을 쓴 뒤 할 일(ESkillBotFollowUp)을 지키는 시간. 스킬 지속 시간이 짧아도 동작이 보이도록 최소 시간을 둔다. */
	LABPROJECT_API float GetFollowUpSeconds(const USkillDefinition& Skill);

	/** 한 스킬이 끝난 뒤 다음 스킬까지 쉬는 시간(무작위). */
	LABPROJECT_API float RollGapBetweenSkills();

	/** 두 액터의 몸 가장자리 사이 수평 거리. 스킬 거리(BotCastRange)와 같은 기준이다. */
	LABPROJECT_API float GetEdgeDistance(const AActor& From, const AActor& To);

	/** From의 눈높이에서 Target이 벽에 가리지 않고 보이는지. */
	LABPROJECT_API bool HasLineOfSight(const AActor& From, const AActor& Target);

	/** From이 Target을 바라보는 수평 방향(Yaw). */
	LABPROJECT_API float GetYawToTarget(const AActor& From, const AActor& Target);

	/**
	 * 대상 둘레를 뛰어다니는 방향. 대상에게서 몸 반지름 + 일정 거리 떨어진 원을 따라 전속력으로 돌고,
	 * 가끔 또는 막혀 서면 도는 방향을 바꿔 장판이 대상 둘레에 고루 깔리게 한다. 봇마다 하나씩 들고 매 프레임 갱신한다.
	 */
	struct LABPROJECT_API FRoamAroundTarget
	{
		/** 원을 도는 방향을 다시 정하고 처음 바꿀 시각을 잡는다. */
		void Start(double Now);

		/** 이번 프레임에 누를 이동 방향(길이 1). */
		FVector Update(const APawn& Self, const AActor& Target, double Now);

	private:
		float CircleSign = 1.0f;
		double NextTurnTime = 0.0;
		double StuckSince = -1.0;
	};
}
