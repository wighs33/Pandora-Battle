#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"

class AActor;
class ACharacterBase;
class USkeletalMeshComponent;
struct FUnarmedCombatSettings;

/**
 * 맨손 공격 판정 창 동안 손·발 소켓 구간을 상자로 쓸어 새로 맞은 상대 캐릭터를 찾는다.
 * 지난 검사 위치에서 지금 위치까지 나눠 검사해 빠른 휘두르기도 놓치지 않고, 한 공격 구간에서 같은 대상은 한 번만 맞힌다.
 */
class FUnarmedAttackSweep
{
public:
	/** 판정 간격·보간 거리·순간이동 거리와 검사 대상이 모두 유효해야 판정을 열 수 있다. */
	static bool CanSweep(const FUnarmedCombatSettings& Settings);

	/** 몽타주가 새 공격 구간에 들어가면 맞은 대상과 지난 검사 위치를 비운다. */
	void EnterSection(FName AttackSectionName, int32 TraceCount);

	/** 공격이 새로 시작될 때 구간 기록을 처음 상태로 돌린다. */
	void ResetHitTracking(int32 TraceCount);

	void Begin(int32 TraceCount);
	void End();
	bool IsActive() const { return bActive; }

	/**
	 * 판정 구간마다 상자 검사를 하고, 같은 편이 아니고 이번 구간에 아직 맞지 않은 캐릭터를 OnNewHit에 넘긴다.
	 * 피해 처리 중에 판정이 끝나거나 다시 열리면 남은 검사를 멈춘다.
	 */
	void Sweep(
		const UObject& WorldContext,
		const FUnarmedCombatSettings& Settings,
		ACharacterBase& SourceCharacter,
		USkeletalMeshComponent& SourceMesh,
		bool bDrawDebug,
		TFunctionRef<void(AActor*)> OnNewHit);

private:
	void ResetPreviousTraces(int32 TraceCount);

	TSet<TWeakObjectPtr<AActor>> HitActorsInSection;
	FName TrackedSectionName = NAME_None;
	TArray<AActor*> ActorsToIgnore;
	TArray<FHitResult> HitResults;
	TArray<FHitResult> InterpolatedHitResults;
	TArray<FVector> PreviousTraceStartLocations;
	TArray<FVector> PreviousTraceEndLocations;
	TArray<uint8> PreviousTraceValid;
	uint32 Generation = 0;
	bool bActive = false;
};
