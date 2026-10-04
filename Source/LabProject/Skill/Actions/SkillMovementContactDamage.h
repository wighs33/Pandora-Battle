#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "Engine/OverlapResult.h"
#include "TimerManager.h"
#include "UObject/ObjectKey.h"

class USkillAbility;
class UWorld;

/**
 * 이동 스킬이 움직이는 동안 캡슐보다 조금 큰 범위에 새로 닿은 적에게 스킬 피해를 준다.
 *
 * 쓰는 액션이 시작할 때 Start, 끝나거나 취소될 때 Stop을 부르고, 판정 타이머와 닿은 대상 기록은 이 객체가 모두 가진다.
 * 닿아 있는 동안 같은 적은 한 번만 맞고, 떨어졌다가 다시 닿으면 다시 맞는다. 서버에서만 판정한다.
 */
class FSkillMovementContactDamage
{
public:
	/** 스킬 정의가 접촉 피해를 켰고 피해량이 있으면 판정을 시작한다. TimerOwner가 사라지면 타이머도 불리지 않는다. */
	void Start(USkillAbility& SkillAbility, UObject& TimerOwner);
	void Stop();

private:
	void Tick();
	void ApplyDamageTo(AActor* HitActor);

	TWeakObjectPtr<USkillAbility> Ability;
	TWeakObjectPtr<UWorld> TimerWorld;
	FTimerHandle TimerHandle;
	bool bActive = false;
	FVector PreviousLocation = FVector::ZeroVector;
	TSet<FObjectKey> OverlappingActors;
	TSet<FObjectKey> CurrentActors;
	TArray<FHitResult> SweepHits;
	TArray<FOverlapResult> OverlapResults;
};
