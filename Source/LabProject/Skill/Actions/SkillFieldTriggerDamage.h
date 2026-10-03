#pragma once

#include "CoreMinimal.h"
#include "TimerManager.h"
#include "UObject/Object.h"
#include "UObject/ObjectKey.h"
#include "SkillFieldTriggerDamage.generated.h"

class AActor;
class UPrimitiveComponent;
struct FHitResult;

/** 트리거에 겹친 대상에게 피해를 줄 차례가 되면 부른다. 피해를 시도했으면 true를 돌려준다. */
DECLARE_DELEGATE_RetVal_TwoParams(bool, FSkillFieldTriggerHit, AActor* /*DamageSourceActor*/, AActor* /*HitActor*/);

/**
 * 배치 액터의 트리거 볼륨에 겹친 캐릭터를 추적하고 피해를 줄 시점을 정한다.
 * 기본은 배치 액터마다 대상당 한 번이고, 반복 설정이면 겹쳐 있는 동안 주기마다 다시 맞힌다.
 * 피해 계산과 적용은 OnHit을 넘긴 쪽이 맡는다.
 */
UCLASS()
class LABPROJECT_API USkillFieldTriggerDamage : public UObject
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	void Configure(FName InTriggerComponentName, bool bInRepeatWhileOverlapping, double InRepeatInterval, FSkillFieldTriggerHit InOnHit);

	/** 배치 액터의 트리거 볼륨에 겹침 피해를 연결한다. 이미 겹쳐 있는 대상은 bDamageExistingOverlaps일 때만 바로 맞힌다. */
	void Bind(AActor* FieldActor, bool bDamageExistingOverlaps);

	/** 모든 연결을 끊고 트리거 충돌을 끈 뒤 추적 상태를 비운다. */
	void Reset();

	bool IsBound(const AActor* FieldActor) const;

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
	UPrimitiveComponent* FindTriggerComponent(AActor* FieldActor) const;
	void StartRepeatTickIfNeeded();
	void Track(AActor* DamageSourceActor, AActor* OtherActor);
	void Untrack(AActor* DamageSourceActor, AActor* OtherActor);
	void Hit(AActor* DamageSourceActor, AActor* HitActor, bool bAllowRepeatedDamage);

private:
	FName TriggerComponentName;
	bool bRepeatWhileOverlapping = false;
	double RepeatInterval = 0.0;
	FSkillFieldTriggerHit OnHit;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPrimitiveComponent>> TriggerComponents;

	FTimerHandle RepeatTimerHandle;
	TMap<FObjectKey, TSet<FObjectKey>> DamagedActorsBySource;
	TMap<FObjectKey, TWeakObjectPtr<AActor>> DamageSourceActorsByKey;
	TMap<FObjectKey, TArray<TWeakObjectPtr<AActor>>> OverlappingActorsBySource;
};
