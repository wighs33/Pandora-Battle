#pragma once

#include "Components/ActorComponent.h"
#include "Definition/Character/EnemyBaseDefinition.h"
#include "GameplayAbilitySpecHandle.h"
#include "EnemyCombatComponent.generated.h"

class AEnemyBase;
class UItemDefinition;
class FContentLease;

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

	/** 조종 중인 AI 컨트롤러가 고른 대상. 없으면 설정에 따라 가장 가까운 플레이어를 쓴다. */
	AActor* ResolveAttackTarget() const;
	bool IsActorValidAttackTarget(const AActor* InActor) const;

	void SetUseNearestPlayerWhenTargetUnset(bool bInUseNearestPlayer);

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

	// 서버에서 기본 능력치 준비 경로를 한 번이라도 거쳤는지 확인한다. 속성 집합 자체는 적 액터의 기본 서브오브젝트다.
	bool IsDefaultAttributeSetupComplete() const { return bDefaultAttributeSetupPerformed; }
	bool EquipEnemyWeaponDefinition(const UItemDefinition* WeaponDefinition);
	const UItemDefinition* GetCurrentOrStartingEnemyWeaponDefinition() const;
	void ClearStartingWeaponDefinition();
	void ResetAttributesForRespawn();

	// Event Handlers --------------------------------------------------------------------------------------------------
	void HandlePossessed();

private:
	void EnsureDefaultAttributeSetup();
	bool ApplyDefaultStatDefinition();
	bool EquipStartingWeapon();
	void HandleInitialCombatDelayElapsed();
	void HandleRuntimeContentPreloaded();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	AEnemyBase* GetEnemyOwner() const;
	const AEnemyBase* GetEnemyOwnerConst() const;
	void StartAttackTimer();
	void BeginRuntimeContentPreload();
	bool ValidateAttackRequest();
	void StopAttackMovement() const;
	void FaceAttackTarget(const AActor* CurrentAttackTarget);

private:
	UPROPERTY(Transient)
	FEnemyCombatSettings Settings;

	FTimerHandle InitialCombatTimerHandle;
	FTimerHandle AttackTimerHandle;
	TSharedPtr<FContentLease> RuntimeContentLease;
	bool bRuntimeContentReady = true;
	bool bHandlePossessedWhenContentReady = false;

	bool bDefaultAttributeSetupPerformed = false;
	bool bDefaultStatDefinitionApplied = false;
};
