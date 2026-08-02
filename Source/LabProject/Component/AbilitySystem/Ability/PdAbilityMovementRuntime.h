#pragma once

#include "CoreMinimal.h"
#include "Engine/OverlapResult.h"
#include "TimerManager.h"
#include "UObject/Object.h"
#include "UObject/ObjectKey.h"
#include "PdAbilityMovementRuntime.generated.h"

class UPdGameplayAbility;
struct FGameplayEffectSpecHandle;

/**
 * Owns movement locking and movement-contact damage for one ability instance.
 */
UCLASS()
class LABPROJECT_API UPdAbilityMovementRuntime : public UObject
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

	bool IsMovementLocked() const { return bAbilityMovementLocked; }
	bool IsContactDamageActive() const { return bMovementContactDamageActive; }

private:
	UPdGameplayAbility* GetOwningAbility() const;
	void HandleMovementContactDamageTick();
	void ApplyMovementContactDamageToActor(UPdGameplayAbility& Ability, AActor* HitActor);
	FGameplayEffectSpecHandle MakeMovementContactDamageSpec(
		const UPdGameplayAbility& Ability,
		float DamageMagnitude) const;
	float CalculateMovementContactDamageMagnitude(
		const UPdGameplayAbility& Ability) const;

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
	TSet<FObjectKey> MovementContactProcessedActors;
	TArray<FHitResult> MovementContactSweepHits;
	TArray<FOverlapResult> MovementContactOverlapResults;
};
