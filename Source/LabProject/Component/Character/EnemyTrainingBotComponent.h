#pragma once

#include "Components/ActorComponent.h"
#include "Definition/Character/EnemyBaseDefinition.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnemyTrainingBotComponent.generated.h"

class AEnemyBase;
class UAnimMontage;
class UItemDefinition;

/**
 * 훈련 봇 전용 피격 반응·일시적 이동 잠금·무기 교체·제자리 리스폰 상태를 관리한다.
 * 일반 몬스터에서는 동작하지 않는다.
 */
UCLASS(ClassGroup = (Character), meta = (BlueprintSpawnableComponent))
class LABPROJECT_API UEnemyTrainingBotComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	UEnemyTrainingBotComponent();

	void ApplySettings(const FEnemyTrainingBotSettings& InSettings);
	const FEnemyTrainingBotSettings& GetSettings() const { return Settings; }

	void InitializeRuntime();
	void ShutdownRuntime();

	bool ShouldUseHitReaction() const;
	bool ShouldUseRespawn() const;
	bool ShouldSuppressDeathHandling() const;

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

	// Event Handlers --------------------------------------------------------------------------------------------------
	void HandleDeathAfterBase();
	void HandleDamageTaken(
		float DamageAmount,
		bool bCriticalHit,
		bool bAllowHitReact);

private:
	void EndHitStun();
	void FinishPendingWeaponChange();
	void Respawn();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	AEnemyBase* GetEnemyOwner() const;
	const AEnemyBase* GetEnemyOwnerConst() const;
	void TriggerHitReaction(float DamageAmount, bool bCriticalHit);
	void StartHitStun();
	bool TryActivateHitReactAbility();
	UAnimMontage* ResolveHitReactMontage() const;

	void CancelWeaponChangeAttackState(float BlendOutTime = 0.08f);
	bool PlayCurrentUnequipMontage(float& OutDuration);

	void CacheRespawnTransform();
	void ScheduleRespawn();
	void ResetRuntimeStateForRespawn();
	void CleanupArrowProjectilesForRespawn();

private:
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
