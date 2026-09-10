#pragma once

#include "CoreMinimal.h"
#include "Engine/OverlapResult.h"
#include "TimerManager.h"
#include "UObject/Object.h"
#include "UObject/ObjectKey.h"
#include "AbilityMovementManager.generated.h"

class UPdGameplayAbility;

/** 한 능력의 이동 잠금·복구와 이동 중 접촉 피해를 관리한다. */
UCLASS()
class LABPROJECT_API UAbilityMovementManager : public UObject
{
	GENERATED_BODY()

public:
	void StopAvatarMovementForSkillActivation(UPdGameplayAbility& Ability);
	void LockAvatarMovementForAbility(UPdGameplayAbility& Ability);
	void RestoreAvatarMovementForAbility(UPdGameplayAbility& Ability);
	void StartDurationMovementLock(UPdGameplayAbility& Ability);
	void StopDurationMovementLock(UPdGameplayAbility& Ability);

	void StartMovementContactDamage(UPdGameplayAbility& Ability);
	void StopMovementContactDamage(UPdGameplayAbility& Ability);

private:
	void HandleMovementContactDamageTick();
	void ApplyMovementContactDamageToActor(UPdGameplayAbility& Ability, AActor* HitActor);

	UPROPERTY(Transient)
	uint8 CachedAbilityMovementMode = 0;

	UPROPERTY(Transient)
	uint8 CachedAbilityCustomMovementMode = 0;

	UPROPERTY(Transient)
	bool bCachedAbilityOrientRotationToMovement = true;

	UPROPERTY(Transient)
	bool bCachedAbilityUseControllerDesiredRotation = false;

	UPROPERTY(Transient)
	bool bCachedAbilityUseControllerRotationYaw = false;

	UPROPERTY(Transient)
	FRotator CachedAbilityRotationRate = FRotator::ZeroRotator;

	UPROPERTY(Transient)
	bool bAbilityMovementLocked = false;

	UPROPERTY(Transient)
	bool bDurationMovementLockActive = false;

	UPROPERTY(Transient)
	bool bMovementContactDamageActive = false;

	UPROPERTY(Transient)
	FVector MovementContactDamagePreviousLocation = FVector::ZeroVector;

	FTimerHandle MovementContactDamageTimerHandle;
	TSet<FObjectKey> MovementContactOverlappingActors;
	TSet<FObjectKey> MovementContactCurrentActors;
	TArray<FHitResult> MovementContactSweepHits;
	TArray<FOverlapResult> MovementContactOverlapResults;
};
