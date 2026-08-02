#pragma once

#include "Components/ActorComponent.h"
#include "Definition/Character/EnemyBaseDefinition.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnemyTrainingBotComponent.generated.h"

class AEnemyBase;
class UAnimMontage;
class UItemDefinition;

/**
 * Owns training-bot-only hit reaction, temporary movement lock, weapon swap,
 * and in-place respawn state. It stays inert for regular monsters.
 */
UCLASS(ClassGroup = (Character), meta = (BlueprintSpawnableComponent))
class LABPROJECT_API UEnemyTrainingBotComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnemyTrainingBotComponent();

	void ApplySettings(const FEnemyTrainingBotSettings& InSettings);
	const FEnemyTrainingBotSettings& GetSettings() const { return Settings; }

	void InitializeRuntime();
	void ShutdownRuntime();

	bool ShouldUseHitReaction() const;
	bool ShouldUseRespawn() const;
	bool ShouldSuppressDeathHandling() const;
	void HandleDeathAfterBase();
	void HandleDamageTaken(
		float DamageAmount,
		bool bCriticalHit,
		bool bAllowHitReact);

	bool RequestWeaponChange(const UItemDefinition* WeaponDefinition);
	bool RequestUnarmed();

	bool IsHitStunned() const { return bHitStunned; }
	bool IsWeaponChangeInProgress() const
	{
		return bWeaponChangeInProgress;
	}

	void PlayHitReactMontageLocal() const;
	void PlayUnequipMontageLocal(
		UAnimMontage* UnequipMontage,
		float PlayRate) const;
	void ResetRespawnVisualsLocal(const FTransform& RespawnTransform);

private:
	AEnemyBase* GetEnemyOwner() const;
	const AEnemyBase* GetEnemyOwnerConst() const;
	void TriggerHitReaction(float DamageAmount, bool bCriticalHit);
	void StartHitStun();
	void EndHitStun();
	bool TryActivateHitReactAbility();
	UAnimMontage* ResolveHitReactMontage() const;

	void CancelWeaponChangeAttackState(float BlendOutTime = 0.08f);
	bool PlayCurrentUnequipMontage(float& OutDuration);
	void FinishPendingWeaponChange();

	void CacheRespawnTransform();
	void ScheduleRespawn();
	void Respawn();
	void ResetRuntimeStateForRespawn();
	void CleanupArrowProjectilesForRespawn();

	UPROPERTY(Transient)
	FEnemyTrainingBotSettings Settings;

	FTimerHandle HitStunTimerHandle;
	FTimerHandle RespawnTimerHandle;
	FTimerHandle WeaponChangeTimerHandle;

	UPROPERTY(Transient)
	bool bHitStunned = false;

	UPROPERTY(Transient)
	TEnumAsByte<EMovementMode> PreHitStunMovementMode = MOVE_Walking;

	UPROPERTY(Transient)
	uint8 PreHitStunCustomMovementMode = 0;

	UPROPERTY(Transient)
	FTransform RespawnTransform = FTransform::Identity;

	UPROPERTY(Transient)
	bool bHasRespawnTransform = false;

	UPROPERTY(Transient)
	TObjectPtr<const UItemDefinition> PendingWeaponDefinition;

	UPROPERTY(Transient)
	bool bPendingUnarmed = false;

	UPROPERTY(Transient)
	bool bWeaponChangeInProgress = false;

	UPROPERTY(Transient)
	bool bRespawnScheduled = false;

	UPROPERTY(Transient)
	bool bRespawnResetInProgress = false;
};
