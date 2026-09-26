#pragma once

#include "Components/ActorComponent.h"
#include "Definition/Character/EnemyBaseDefinition.h"
#include "GameplayAbilitySpecHandle.h"
#include "EnemyCombatComponent.generated.h"

class AEnemyBase;
class UItemDefinition;
struct FStreamableHandle;
struct FAttackData;

/**
 * 적의 대상 지정·능력치 초기화·기본 장비·공격 예약을 관리한다.
 * AEnemyBase는 기존 블루프린트와 StateTree의 진입점으로 유지한다.
 */
UCLASS(ClassGroup = (Character), meta = (BlueprintSpawnableComponent))
class LABPROJECT_API UEnemyCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	UEnemyCombatComponent();

	void ApplySettings(const FEnemyCombatSettings& InSettings);
	const FEnemyCombatSettings& GetSettings() const { return Settings; }
	void ShutdownRuntime();
	void InitializeBehaviorTreeCombat();
	bool IsRuntimeContentReady() const { return bRuntimeContentReady; }

	void SetAttackTarget(AActor* InAttackTarget);
	AActor* GetCachedAttackTarget() const;
	AActor* ResolveAttackTarget() const;
	bool IsActorValidAttackTarget(const AActor* InActor) const;

	void SetUseNearestPlayerWhenTargetUnset(bool bInUseNearestPlayer);
	bool GetUseNearestPlayerWhenTargetUnset() const
	{
		return Settings.bUseNearestPlayerWhenTargetUnset;
	}

	void Attack();
	void SetAttackEnabled(bool bInAttackEnabled);
	bool IsAttackEnabled() const { return Settings.bAttackEnabled; }
	bool IsAttackAbilityActive() const;
	bool IsAttackInProgress() const;
	bool RequestMoveToAttackTarget(AActor* InAttackTarget);
	bool MoveToAttackTarget(AActor* CurrentAttackTarget);

	float GetAttackDistanceToActor(const AActor* InActor) const;
	float GetAttackStartDistance() const;
	bool IsUsingRangedWeapon() const;
	bool IsUsingGunWeapon() const;

	void EnsureDefaultAttributeSetup();
	// 실제 기본 속성 집합이 ASC에 준비되었는지 확인한다.
	bool IsDefaultAttributeSetupComplete() const;
	bool ApplyDefaultStatDefinition();
	bool EquipStartingWeapon();
	bool EquipEnemyWeaponDefinition(const UItemDefinition* WeaponDefinition);
	const UItemDefinition* GetCurrentOrStartingEnemyWeaponDefinition() const;
	void ClearStartingWeaponDefinition();
	void ResetAttributesForRespawn();

	// Event Handlers --------------------------------------------------------------------------------------------------
	void HandlePossessed();

private:
	void HandleInitialCombatDelayElapsed();
	void HandleRuntimeContentPreloaded(uint32 RequestGeneration);

	// Internal Helpers ------------------------------------------------------------------------------------------------
	AEnemyBase* GetEnemyOwner() const;
	const AEnemyBase* GetEnemyOwnerConst() const;
	void StartAttackTimer();
	void BeginRuntimeContentPreload();
	void ReleaseRuntimeContentPreload();
	bool ValidateAttackRequest();
	void StopAttackMovement() const;
	void FaceAttackTarget(const AActor* CurrentAttackTarget);
	void GatherAttackAbilityHandles(
		bool bUsingRangedWeapon,
		bool bHasEquippedWeapon,
		TArray<FGameplayAbilitySpecHandle>& OutAbilityHandles) const;
	bool TryHandleActiveAttackAbility(
		const TArray<FGameplayAbilitySpecHandle>& AbilityHandles);
	bool TryActivateAttackAbility(
		TArray<FGameplayAbilitySpecHandle>& AbilityHandles);

private:
	UPROPERTY(Transient)
	FEnemyCombatSettings Settings;

	UPROPERTY(Transient)
	TObjectPtr<AActor> AttackTarget;

	FTimerHandle InitialCombatTimerHandle;
	FTimerHandle AttackTimerHandle;
	TSharedPtr<FStreamableHandle> RuntimeContentLoadHandle;
	uint32 RuntimeContentLoadGeneration = 0;
	bool bRuntimeContentReady = true;
	bool bHandlePossessedWhenContentReady = false;

	bool bDefaultStatDefinitionApplied = false;
};
