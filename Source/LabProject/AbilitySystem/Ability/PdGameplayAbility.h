#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayTagContainer.h"
#include "Abilities/GameplayAbility.h"
#include "Definition/AbilitySystem/SkillTypes.h"
#include "PdGameplayAbility.generated.h"

class ACharacterBase;
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
class UPandoraSkillSource;
class UAbilityMovementManager;
class UAbilityPresentationManager;
class UAbilityCostAndCooldownManager;
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

	// GAS 엔진 콜백
	virtual bool CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	using Super::GetCooldownTimeRemaining;
	virtual float GetCooldownTimeRemaining(const FGameplayAbilityActorInfo* ActorInfo) const override;
	virtual void GetCooldownTimeRemainingAndDuration(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		float& TimeRemaining, float& CooldownDuration) const override;

	// 출처·캐릭터·입력 정책
	ACharacterBase* GetPdCharacterFromActorInfo() const;
	UPdAbilitySystemComponent* GetPdAbilitySystemComponentFromActorInfo() const;

	// 능력을 부여하거나 부활 후 자동 능력을 재개할 때, 별도 입력 없이 활성화를 시도할 대상인지 알려 준다.
	bool ShouldAutoActivateWhenGranted() const { return bAutoActivateWhenGranted;}

	// 단계형 스킬은 키를 떼는 대신 기본 공격 같은 별도 입력으로 시전을 확정할 수 있다.
	virtual bool ShouldConfirmTargetingOnInputRelease() const;

	// 외부 시스템이 요청하는 쿨다운·연출 정리
	void AppendCooldownRemovalPolicyTags(FGameplayEffectSpecHandle& CooldownSpecHandle) const;

	// GameFeature가 능력을 부여할 때 연결할 기본 입력 태그를 제공한다. 기본은 비어 있고 Grapple 등 파생 능력이 지정한다.
	virtual FGameplayTag GetDefaultInputTag() const { return FGameplayTag(); }

	void DisableCooldownOnAbilityEnd() const;
	void DestroyActiveSkillPresentationActor();

	USkillDefinition* GetSourceSkillDataAsset() const;

	UPandoraSkillSource* GetPandoraSkillSource() const;

	// 능력 실행과 GameplayEffect 적용·조회
	bool TryActivateAbilitiesByTags(FGameplayTagContainer InAbilityTags, bool bAllowRemoteActivation = true) const;
	bool ApplyGameplayEffect(TSubclassOf<UGameplayEffect> GameplayEffectClass, float EffectLevel = 1.0f, int32 StackCount = 1);
	bool RemoveGameplayEffect(TSubclassOf<UGameplayEffect> GameplayEffectClass);

	int32 RemoveGameplayEffectsWithGrantedTags(const FGameplayTagContainer& GrantedTags);
	bool HasPlayerController() const;

	AActor* GetAttackTargetFromAvatar() const;

protected:
	// GAS 실행 시작·확정·종료
	virtual void PreActivate(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate,
		const FGameplayEventData* TriggerEventData = nullptr) override;
	virtual bool CommitAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, FGameplayTagContainer* OptionalRelevantTags = nullptr) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override final;

	// GAS 비용·쿨다운 확장
	virtual const FGameplayTagContainer* GetCooldownTags() const override;
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;
	virtual bool CheckCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;

	// 종료 검사와 잠금 해제 후에만 호출된다. 파생 능력은 엔진 종료 함수를 직접 재정의하지 않는다.
	virtual void OnAbilityEnding();
	// GAS가 능력을 비활성화한 뒤 다음 행동을 이어야 하는 경우에만 사용한다.
	virtual void OnAbilityEnded(bool bWasCancelled);

	void FinishAbilityFromDuration();
	bool CanExecuteSkillPayload() const;
	void CancelAbilityForSkillExecutionFailure();

	bool HasActiveGameplayEffect(TSubclassOf<UGameplayEffect> GameplayEffectClass) const;

	AWeaponBase* GetCurrentWeaponActorFromAvatar() const;
	bool HasCurrentWeaponSkillTrail() const;
	bool StartCurrentWeaponSkillTrail(UNiagaraSystem* TrailSystem) const;
	void StopCurrentWeaponSkillTrail() const;

	void ApplyCooldownImmediately(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const;
	bool ApplySharedCooldownEffect(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, float CooldownDuration, const FGameplayTagContainer& CooldownTags) const;
	bool TryCommitAdditionalActionStaminaCost() const;
	float CalculateBaseSkillDamageMagnitude(const FSkillGameplayEffectConfig& DamageConfig) const;
	float ApplyIntelligenceToSkillDamage(float DamageMagnitude) const;
	float CalculateSkillDamageMagnitude(const FSkillGameplayEffectConfig& DamageConfig) const;

	UAbilityTask_PlayMontageAndWait* CreateDefaultMontageAndWaitTask(UAnimMontage* MontageToPlay);
	UAbilityTask_WaitGameplayEvent* CreateWaitGameplayEventTask(
		const FGameplayTag& EventTag, bool bOnlyTriggerOnce = false, bool bOnlyMatchExact = true);
	AGameplayAbilityTargetActor* BeginSpawningTargetDataActor(
		UAbilityTask_WaitTargetData* TargetDataTask, TSubclassOf<AGameplayAbilityTargetActor> TargetActorClass);
	void FinishSpawningTargetDataActor(UAbilityTask_WaitTargetData* TargetDataTask, AGameplayAbilityTargetActor* SpawnedActor);

	FGameplayEffectSpecHandle MakeConfiguredDamageEffectSpec(
		const FSkillGameplayEffectConfig& DamageConfig, float DamageMagnitude, UObject* SourceObject = nullptr) const;
	FGameplayEffectSpecHandle MakeConfiguredStatusEffectSpec(const USkillDefinition* SkillDataAsset,
		TSubclassOf<UGameplayEffect> FallbackStatusEffectClass = nullptr, float FallbackStatusEffectLevel = 1.0f) const;
	FActiveGameplayEffectHandle ApplyConfiguredStatusEffectToTarget(const USkillDefinition* SkillDataAsset,
		UAbilitySystemComponent* TargetAbilitySystemComponent, TSubclassOf<UGameplayEffect> FallbackStatusEffectClass = nullptr,
		float FallbackStatusEffectLevel = 1.0f) const;

	void LockAvatarMovementForAbility();
	void RestoreAvatarMovementForAbility();
	void StartDurationMovementLock();

	// 파생 능력은 개별 연출을 직접 요청하고, 공통 종료 정리는 부모 능력이 보장한다.
	UAbilityPresentationManager& GetPresentationManager();
	const UAbilityPresentationManager& GetPresentationManager() const;

	void StartConfiguredSelfBuff(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo);
	void StopConfiguredSelfBuff();
	void StartMovementContactDamage();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Cooldown|Policy", meta = (Categories = "Effect.Policy"))
	FGameplayTagContainer CooldownRemovalPolicyTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Activation")
	bool bAutoActivateWhenGranted = false;

private:
	friend class UAbilityMovementManager;
	friend class UAbilityPresentationManager;
	friend class UAbilityCostAndCooldownManager;

	static const USkillDefinition* ResolveSourceSkillDataAsset(UObject* SourceObject);

	bool bIsCleaningUpAbility = false;
	FActiveGameplayEffectHandle ActiveSelfBuffEffectHandle;
	TWeakObjectPtr<UAbilitySystemComponent> SelfBuffAbilitySystemComponent;
	TWeakObjectPtr<UCombatComponent> SelfBuffCombatComponent;
	TWeakObjectPtr<AWeaponBase> SelfBuffTraceEndZWeapon;

	UPROPERTY(VisibleAnywhere, Instanced, Category = "!Ability|CostAndCooldown")
	TObjectPtr<UAbilityCostAndCooldownManager> CostAndCooldownManager;

	UPROPERTY(VisibleAnywhere, Instanced, Category = "!Ability|Movement")
	TObjectPtr<UAbilityMovementManager> MovementManager;

	UPROPERTY(VisibleAnywhere, Instanced, Category = "!Ability|Presentation")
	TObjectPtr<UAbilityPresentationManager> PresentationManager;
};
