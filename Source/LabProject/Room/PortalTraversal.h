#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectKey.h"

class AActor;
class UPrimitiveComponent;

/**
 * 포털 평면을 앞에서 뒤로 넘은 순간을 액터마다 판정한다.
 * 지난 위치는 평면 앞, 이번 위치는 평면 뒤여야 넘은 것으로 보고, 순간 이동 직후에는 대기 시간 동안 다시 넘지 않는다.
 */
class FPortalTraversalTracker
{
public:
	// 평면을 앞에서 뒤로 넘었고 대기 시간이 지났으면 true다. 처음 보는 액터는 위치만 기록한다.
	bool UpdateCrossing(
		const AActor* Actor,
		const FVector& Point,
		const FVector& PlaneLocation,
		const FVector& PlaneNormal,
		double Now,
		float Cooldown,
		float CrossingTolerance);

	// 순간 이동 직후 위치와 시각을 기록해, 같은 액터가 곧바로 다시 넘지 않게 한다.
	void Prime(const AActor* Actor, const FVector& Location, double Now);

	void Remove(const AActor* Actor);
	void Reset();

private:
	struct FState
	{
		/** 직전에 확인한 위치. 비어 있으면 아직 확인한 적이 없다. */
		TOptional<FVector> LastPosition;
		/** 마지막으로 순간 이동한 시각. 비어 있으면 이동한 적이 없다. */
		TOptional<double> LastTeleportTime;
	};

	TMap<TObjectKey<AActor>, FState> States;
};

/** 포털 상자에 겹친 순간 이동 후보를 약한 참조로 모아 둔다. 누가 후보인지는 포털이 정한다. */
class FPortalOverlapList
{
public:
	using FCandidateFilter = TFunctionRef<bool(const AActor*)>;

	// 상자에 이미 겹쳐 있는 후보로 목록을 다시 채운다.
	void Seed(const UPrimitiveComponent* OverlapComponent, FCandidateFilter IsCandidate);

	// 후보면 목록에 넣고 true를 돌려준다. 이미 있으면 그대로 true다.
	bool Track(AActor* Actor, FCandidateFilter IsCandidate);

	// 상자를 벗어난 액터와 사라진 참조를 지운다. 다른 몸체로 아직 겹쳐 있으면 남긴다.
	void Untrack(AActor* Actor, const UPrimitiveComponent* OverlapComponent);

	// 상자에 겹친 후보가 하나라도 남았는지. 벗어났거나 후보가 아닌 항목은 지운다.
	bool HasAny(const UPrimitiveComponent* OverlapComponent, FCandidateFilter IsCandidate);

	// 상자에 겹친 후보를 모은다. 벗어난 항목은 지우고 OnDropped에 알린다.
	void Collect(
		const UPrimitiveComponent* OverlapComponent,
		FCandidateFilter IsCandidate,
		TArray<AActor*>& OutActors,
		TFunctionRef<void(AActor*)> OnDropped);

	void Reset();

private:
	TArray<TWeakObjectPtr<AActor>> Actors;
};
