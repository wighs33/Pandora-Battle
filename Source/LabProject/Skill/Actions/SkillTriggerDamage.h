#pragma once

#include "CoreMinimal.h"
#include "TimerManager.h"
#include "UObject/Object.h"
#include "UObject/ObjectKey.h"
#include "SkillTriggerDamage.generated.h"

class AActor;
class UPrimitiveComponent;
struct FHitResult;
struct FSkillTopLevelDamageConfig;

/** 트리거에 겹친 대상에게 피해를 줄 차례가 되면 부른다. 피해를 시도했으면 true를 돌려준다. */
DECLARE_DELEGATE_RetVal_TwoParams(bool, FSkillTriggerHit, AActor* /*DamageSourceActor*/, AActor* /*HitActor*/);

/**
 * 배치·소환 액터의 트리거 볼륨에 겹친 캐릭터를 추적하고 피해를 줄 시점을 정한다.
 * 기본은 트리거 소유 액터마다 대상당 한 번이고, 반복 설정이면 겹쳐 있는 동안 주기마다 다시 맞힌다.
 * 피해는 켜진 상태로 시작하며, 꺼 두는 동안에도 겹침은 계속 추적한다.
 * 트리거의 충돌 설정과 피해 계산·적용은 부르는 쪽이 맡는다.
 */
UCLASS()
class LABPROJECT_API USkillTriggerDamage : public UObject
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	void Configure(const FSkillTopLevelDamageConfig& DamageConfig, FSkillTriggerHit InOnHit);

	/**
	 * 이름이 같은 컴포넌트, 같은 태그, 이름을 포함하는 컴포넌트 순으로 찾는다.
	 * bUseAnyPrimitiveAsFallback이면 겹침을 받는 첫 충돌체, 그것도 없으면 첫 충돌체를 쓴다.
	 */
	static UPrimitiveComponent* FindTriggerComponent(AActor* Actor, FName ComponentName, bool bUseAnyPrimitiveAsFallback);

	/** 트리거 볼륨에 겹침 추적을 연결한다. 이미 겹쳐 있는 대상은 bHitExistingOverlaps이고 피해가 켜져 있을 때만 바로 맞힌다. */
	void Bind(UPrimitiveComponent* TriggerComponent, bool bHitExistingOverlaps);

	/** 피해를 켜면 지금 겹쳐 있는 대상을 바로 맞히고 반복 주기를 시작한다. 끄면 반복 주기를 멈춘다. */
	void SetDamageActive(bool bActive);

	/** 모든 연결을 끊고 트리거 충돌을 끈 뒤 추적 상태를 비운다. 피해는 다시 켜진 상태가 된다. */
	void Reset();

	bool IsBound(const AActor* TriggerOwner) const;

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	UFUNCTION()
	void HandleBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	void HandleRepeatTick();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void TrackExistingOverlaps(UPrimitiveComponent* TriggerComponent, bool bHitExistingOverlaps);
	void StartRepeatTickIfNeeded();
	void Track(AActor* DamageSourceActor, AActor* OtherActor);
	void Untrack(AActor* DamageSourceActor, AActor* OtherActor);
	void Hit(AActor* DamageSourceActor, AActor* HitActor, bool bAllowRepeatedDamage);

private:
	bool bRepeatWhileOverlapping = false;
	double RepeatInterval = 0.0;
	bool bDamageActive = true;
	FSkillTriggerHit OnHit;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPrimitiveComponent>> TriggerComponents;

	FTimerHandle RepeatTimerHandle;
	TMap<FObjectKey, TSet<FObjectKey>> DamagedActorsBySource;
	TMap<FObjectKey, TWeakObjectPtr<AActor>> DamageSourceActorsByKey;
	TMap<FObjectKey, TArray<TWeakObjectPtr<AActor>>> OverlappingActorsBySource;
};
