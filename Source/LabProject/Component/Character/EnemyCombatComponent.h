#pragma once

#include "Components/ActorComponent.h"
#include "Definition/Character/EnemyBaseDefinition.h"
#include "GameplayAbilitySpecHandle.h"
#include "EnemyCombatComponent.generated.h"

class AEnemyBase;
class UGameplayAbility;
class UItemDefinition;
struct FStreamableHandle;
struct FAttackData;

/**
 * Owns enemy targeting, ability/stat bootstrapping, equipment defaults, and
 * attack scheduling. AEnemyBase remains the stable Blueprint/StateTree facade.
 */
UCLASS(ClassGroup = (Character), meta = (BlueprintSpawnableComponent))
class LABPROJECT_API UEnemyCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnemyCombatComponent();

	void ApplySettings(const FEnemyCombatSettings& InSettings);
	const FEnemyCombatSettings& GetSettings() const { return Settings; }

	void HandlePossessed();
	void ShutdownRuntime();
	void InitializeBehaviorTreeCombat();

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
	bool IsDefaultAttributeSetupComplete() const
	{
		return DefaultAttributeConfigHandle != INDEX_NONE;
	}
	bool ApplyDefaultStatDefinition();
	bool EquipStartingWeapon();
	bool EquipEnemyWeaponDefinition(const UItemDefinition* WeaponDefinition);
	const UItemDefinition* GetCurrentOrStartingEnemyWeaponDefinition() const;
	void ClearStartingWeaponDefinition();
	void GrantDefaultCombatAbilities();
	void ResetAttributesForRespawn();

private:
	AEnemyBase* GetEnemyOwner() const;
	const AEnemyBase* GetEnemyOwnerConst() const;
	void HandleInitialCombatDelayElapsed();
	void StartAttackTimer();
	void BeginRuntimeContentPreload();
	void HandleRuntimeContentPreloaded(uint32 RequestGeneration);
	void ReleaseRuntimeContentPreload();
	void EnsureDefaultCombatAbilityClasses();
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

	UPROPERTY(Transient)
	TArray<FGameplayAbilitySpecHandle> DefaultCombatAbilityHandles;

	int32 DefaultAttributeConfigHandle = INDEX_NONE;
	bool bDefaultStatDefinitionApplied = false;
};
