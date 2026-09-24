#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "GameplayTagContainer.h"
#include "RangedAttackAbility.generated.h"

class UGameplayEffect;
class AWeaponBase;
class ACharacterBase;

UCLASS(Blueprintable)
class LABPROJECT_API URangedAttackAbility : public UPdGameplayAbility
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual bool CanActivateAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

public:
	// Public API ------------------------------------------------------------------------------------------------------
	URangedAttackAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	// Event Handlers --------------------------------------------------------------------------------------------------
	virtual void OnAbilityEnding() override;

	UFUNCTION()
	void OnAttackMontageCompleted();

	UFUNCTION()
	void OnAttackMontageInterrupted();

	UFUNCTION()
	void OnAttackMontageCancelled();

	UFUNCTION()
	void OnAttackTraceStart(FGameplayEventData Payload);

	UFUNCTION()
	void OnAttackTraceEnd(FGameplayEventData Payload);

	UFUNCTION()
	void HandleAIPrimaryAttackTimer();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void CleanupAttackState();
	AWeaponBase* GetCurrentWeaponActor() const;
	void SetCurrentWeaponTraceEnabled(bool bEnabled) const;
	AActor* ResolveAttackTarget(ACharacterBase* Character) const;
	bool ShouldUseAIWeaponFire(ACharacterBase* Character, AWeaponBase* CurrentWeapon) const;
	bool TryCacheAIPrimaryAttackTarget(ACharacterBase* Character);
	bool TryExecuteScheduledAIWeaponFire();
	FVector ResolveAITargetAimLocation(const AActor* TargetActor) const;
	void FaceCharacterToTargetLocation(ACharacterBase* Character, const FVector& TargetLocation) const;
	float GetAIRangedTargetLockDelay() const;
	void ScheduleAIPrimaryAttack();
	void ClearAIPrimaryAttackTimer();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Effect")
	TSubclassOf<UGameplayEffect> AttackingEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Event", meta = (Categories = "GameplayEvent"))
	FGameplayTag AttackTraceStartEventTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Event", meta = (Categories = "GameplayEvent"))
	FGameplayTag AttackTraceEndEventTag;

	UPROPERTY(Transient)
	bool bAIPrimaryAttackExecuted = false;

	UPROPERTY(Transient)
	bool bHasCachedAIPrimaryAttackTargetLocation = false;

	UPROPERTY(Transient)
	FVector CachedAIPrimaryAttackTargetLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> CachedAIPrimaryAttackTargetActor;

	FTimerHandle AIPrimaryAttackTimerHandle;
};
