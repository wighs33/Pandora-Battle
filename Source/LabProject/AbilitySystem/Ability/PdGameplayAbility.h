#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayTagContainer.h"
#include "Abilities/GameplayAbility.h"
#include "Definition/AbilitySystem/SkillTypes.h"
#include "PdGameplayAbility.generated.h"

class ACharacterBase;
class APdPlayerState;
class AGameplayAbilityTargetActor;
class AWeaponBase;
class UAbilitySystemComponent;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitTargetData;
class UAbilityTask_WaitGameplayEvent;
class UAnimMontage;
class UGameplayEffect;
class UNiagaraSystem;
class USkillDefinition;
class UPandoraSkillRuntimeContext;
class UAbilityMovementRuntime;
class UAbilityPresentationRuntime;
class UAbilityResourceRuntime;
class UPdAbilitySystemComponent;
class UCombatComponent;

/**
 * 프로젝트 능력의 실행 규칙과 공통 API를 제공한다.
 *
 * GAS의 시작·종료, 출처 조회와 자기 버프 적용은 능력이 담당한다.
 * 자원·쿨다운, 이동, 시각효과의 실행 상태는 각 전용 객체에 맡긴다.
 */
UCLASS(Abstract, Blueprintable)
class LABPROJECT_API UPdGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UPdGameplayAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//------------------------------------------------------------------------------------------------------------------
	//--- Engine Callbacks
	virtual bool CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	using Super::GetCooldownTimeRemaining;
	virtual float GetCooldownTimeRemaining(const FGameplayAbilityActorInfo* ActorInfo) const override;
	virtual void GetCooldownTimeRemainingAndDuration(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		float& TimeRemaining, float& CooldownDuration) const override;

	//------------------------------------------------------------------------------------------------------------------

	ACharacterBase* GetPdCharacterFromActorInfo() const;
	APdPlayerState* GetPdPlayerStateFromActorInfo() const;
	UPdAbilitySystemComponent* GetPdAbilitySystemComponentFromActorInfo() const;

	bool ShouldAutoActivateWhenGranted() const { return bAutoActivateWhenGranted; }
	// 단계형 스킬은 키를 떼는 대신 기본 공격 같은 별도 입력으로 시전을 확정할 수 있다.
	virtual bool ShouldAutoConfirmOnInputRelease() const { return true; }

	void AppendCooldownRemovalPolicyTags(FGameplayEffectSpecHandle& CooldownSpecHandle) const;
	virtual FGameplayTag GetDefaultInputTag() const { return FGameplayTag(); }
	void SuppressPendingCooldownForRuntimeReset() const;
	void CleanupConfiguredPresentation();

	USkillDefinition* GetSourceSkillDataAsset() const;

	UPandoraSkillRuntimeContext* GetSourceSkillRuntimeContext() const;

	TArray<FProjectileImpactEffectAreaSpawnConfig> GetSourceProjectileImpactEffectAreas() const;

	bool TryActivateAbilitiesByTags(FGameplayTagContainer InAbilityTags, bool bAllowRemoteActivation = true) const;
	int32 GrantAbilities(const TArray<TSubclassOf<UGameplayAbility>>& AbilityClasses, int32 AbilityLevel = 1);
	bool ApplyGameplayEffect(TSubclassOf<UGameplayEffect> GameplayEffectClass, float EffectLevel = 1.0f, int32 StackCount = 1);
	bool RemoveGameplayEffect(TSubclassOf<UGameplayEffect> GameplayEffectClass);

	int32 RemoveGameplayEffectsWithGrantedTags(const FGameplayTagContainer& GrantedTags);
	bool HasPlayerController() const;

	AActor* GetAttackTargetFromAvatar() const;

protected:
	//------------------------------------------------------------------------------------------------------------------
	//--- Engine Callbacks
	virtual void PreActivate(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate,
		const FGameplayEventData* TriggerEventData = nullptr) override;
	virtual const FGameplayTagContainer* GetCooldownTags() const override;
	virtual bool CheckCost(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCost(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;
	virtual bool CheckCooldown(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ApplyCooldown(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;
	virtual bool CommitAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) override;
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override final;

	//------------------------------------------------------------------------------------------------------------------

	// 종료 검사와 잠금 해제 후에만 호출된다. 파생 능력은 엔진 종료 함수를 직접 재정의하지 않는다.
	virtual void OnAbilityEnding();
	// GAS가 능력을 비활성화한 뒤 다음 행동을 이어야 하는 경우에만 사용한다.
	virtual void OnAbilityEnded(bool bWasCancelled);
	void FinishAbilityFromDuration();
	bool CanExecuteSkillPayload() const;
	void CancelAbilityForSkillExecutionFailure();

	FActiveGameplayEffectHandle ApplyGameplayEffectHandle(
		TSubclassOf<UGameplayEffect> GameplayEffectClass,
		float EffectLevel = 1.0f,
		int32 StackCount = 1);
	FActiveGameplayEffectHandle ApplyGameplayEffectHandle(
		TSubclassOf<UGameplayEffect> GameplayEffectClass,
		const FGameplayTagContainer& DynamicGrantedTags,
		float EffectLevel = 1.0f,
		int32 StackCount = 1);
	bool HasActiveGameplayEffect(TSubclassOf<UGameplayEffect> GameplayEffectClass) const;
	AWeaponBase* GetCurrentWeaponActorFromAvatar() const;
	bool HasCurrentWeaponSkillTrail() const;
	bool StartCurrentWeaponSkillTrail(UNiagaraSystem* TrailSystem) const;
	void StopCurrentWeaponSkillTrail() const;
	void ApplyCooldownImmediately(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const;
	bool ApplySharedCooldownEffect(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		float CooldownDuration,
		const FGameplayTagContainer& CooldownTags) const;
	bool TryCommitAdditionalActionStaminaCost() const;
	float CalculateBaseSkillDamageMagnitude(const FSkillGameplayEffectConfig& DamageConfig) const;
	float ApplyIntelligenceToSkillDamage(float DamageMagnitude) const;
	float CalculateSkillDamageMagnitude(const FSkillGameplayEffectConfig& DamageConfig) const;
	UAbilityTask_PlayMontageAndWait* CreateDefaultMontageAndWaitTask(UAnimMontage* MontageToPlay);
	UAbilityTask_WaitGameplayEvent* CreateWaitGameplayEventTask(
		const FGameplayTag& EventTag,
		bool bOnlyTriggerOnce = false,
		bool bOnlyMatchExact = true);
	AGameplayAbilityTargetActor* BeginSpawningTargetDataActor(
		UAbilityTask_WaitTargetData* TargetDataTask,
		TSubclassOf<AGameplayAbilityTargetActor> TargetActorClass);
	void FinishSpawningTargetDataActor(UAbilityTask_WaitTargetData* TargetDataTask, AGameplayAbilityTargetActor* SpawnedActor);
	FGameplayEffectSpecHandle MakeConfiguredDamageEffectSpec(
		const FSkillGameplayEffectConfig& DamageConfig,
		float DamageMagnitude,
		UObject* SourceObject = nullptr) const;
	FGameplayEffectSpecHandle MakeConfiguredStatusEffectSpec(
		const USkillDefinition* SkillDataAsset,
		TSubclassOf<UGameplayEffect> FallbackStatusEffectClass = nullptr,
		float FallbackStatusEffectLevel = 1.0f) const;
	FActiveGameplayEffectHandle ApplyConfiguredStatusEffectToTarget(
		const USkillDefinition* SkillDataAsset,
		UAbilitySystemComponent* TargetAbilitySystemComponent,
		TSubclassOf<UGameplayEffect> FallbackStatusEffectClass = nullptr,
		float FallbackStatusEffectLevel = 1.0f) const;
	void StopAvatarMovementForSkillActivation();
	void LockAvatarMovementForAbility();
	void RestoreAvatarMovementForAbility();
	void StartDurationMovementLock();
	void StopDurationMovementLock();

	// 파생 능력은 개별 연출을 직접 요청하고, 공통 종료 정리는 부모 능력이 보장한다.
	UAbilityPresentationRuntime& GetPresentationRuntime();
	const UAbilityPresentationRuntime& GetPresentationRuntime() const;

	void StartConfiguredSelfBuff(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo);
	void StopConfiguredSelfBuff();
	void StartMovementContactDamage();
	void StopMovementContactDamage();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Cooldown|Policy",
		meta = (Categories = "Effect.Policy"))
	FGameplayTagContainer CooldownRemovalPolicyTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Activation")
	bool bAutoActivateWhenGranted = false;

private:
	friend class UAbilityMovementRuntime;
	friend class UAbilityPresentationRuntime;
	friend class UAbilityResourceRuntime;

	static const USkillDefinition* ResolveSourceSkillDataAsset(UObject* SourceObject);

	bool bIsEndingAbilityRuntime = false;
	FActiveGameplayEffectHandle ActiveSelfBuffEffectHandle;
	TWeakObjectPtr<UAbilitySystemComponent> SelfBuffAbilitySystemComponent;
	TWeakObjectPtr<UCombatComponent> SelfBuffCombatComponent;
	TWeakObjectPtr<AWeaponBase> SelfBuffTraceEndZWeapon;

	UPROPERTY(VisibleAnywhere, Instanced, Category = "!Ability|Runtime")
	TObjectPtr<UAbilityResourceRuntime> ResourceRuntime;

	UPROPERTY(VisibleAnywhere, Instanced, Category = "!Ability|Runtime")
	TObjectPtr<UAbilityMovementRuntime> MovementRuntime;

	UPROPERTY(VisibleAnywhere, Instanced, Category = "!Ability|Runtime")
	TObjectPtr<UAbilityPresentationRuntime> PresentationRuntime;
};
