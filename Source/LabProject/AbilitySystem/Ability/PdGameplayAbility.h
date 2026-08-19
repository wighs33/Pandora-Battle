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
class UAbilitySourceRuntime;
class UPdAbilitySystemComponent;

/**
 * Project GameplayAbility facade.
 *
 * GAS lifecycle integration and the protected API used by derived abilities
 * remain here. Focused, per-instance runtime objects own mutable resource,
 * source, movement, and presentation behavior.
 */
UCLASS(Abstract, Blueprintable)
class LABPROJECT_API UPdGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UPdGameplayAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	ACharacterBase* GetPdCharacterFromActorInfo() const;
	APdPlayerState* GetPdPlayerStateFromActorInfo() const;
	UPdAbilitySystemComponent* GetPdAbilitySystemComponentFromActorInfo() const;

	bool ShouldAutoActivateWhenGranted() const { return bAutoActivateWhenGranted; }
	// Staged abilities can reserve confirmation for a separate input such as primary attack.
	virtual bool ShouldAutoConfirmOnInputRelease() const { return true; }
	UAbilityResourceRuntime* GetResourceRuntime() const { return ResourceRuntime.Get(); }
	UAbilitySourceRuntime* GetSourceRuntime() const { return SourceRuntime.Get(); }
	UAbilityMovementRuntime* GetMovementRuntime() const { return MovementRuntime.Get(); }
	UAbilityPresentationRuntime* GetPresentationRuntime() const { return PresentationRuntime.Get(); }

	void AppendCooldownRemovalPolicyTags(
		FGameplayEffectSpecHandle& CooldownSpecHandle,
		bool bPandoraCooldown) const;
	virtual FGameplayTag GetDefaultInputTag() const { return FGameplayTag(); }
	void SuppressPendingCooldownForRuntimeReset() const;
	void CleanupConfiguredPresentation();

	USkillDefinition* GetSourceSkillDataAsset() const;

	UPandoraSkillRuntimeContext* GetSourceSkillRuntimeContext() const;

	TArray<FProjectileImpactEffectAreaSpawnConfig> GetSourceProjectileImpactEffectAreas() const;

	bool TryActivateAbilitiesByTags(
		FGameplayTagContainer InAbilityTags,
		bool bAllowRemoteActivation = true) const;
	int32 GrantAbilities(
		const TArray<TSubclassOf<UGameplayAbility>>& AbilityClasses,
		int32 AbilityLevel = 1);
	bool ApplyGameplayEffect(
		TSubclassOf<UGameplayEffect> GameplayEffectClass,
		float EffectLevel = 1.0f,
		int32 StackCount = 1);
	bool RemoveGameplayEffect(TSubclassOf<UGameplayEffect> GameplayEffectClass);

	int32 RemoveGameplayEffectsWithGrantedTags(const FGameplayTagContainer& GrantedTags);
	bool HasPlayerController() const;

	AActor* GetAttackTargetFromAvatar() const;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Cooldown|Policy",
		meta = (Categories = "Effect.Policy"))
	FGameplayTagContainer CooldownRemovalPolicyTags;

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
		bool bWasCancelled) override;
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
	UObject* GetCurrentAbilitySpecSourceObject() const;
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
		const FGameplayTagContainer& CooldownTags,
		bool bPandoraCooldown = false) const;
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
	void FinishSpawningTargetDataActor(
		UAbilityTask_WaitTargetData* TargetDataTask,
		AGameplayAbilityTargetActor* SpawnedActor);
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
	void SpawnConfiguredCharacterDecal();
	void StartConfiguredDefaultFX();
	void StopConfiguredDefaultFX();
	void StartConfiguredGroundFX();
	void StartConfiguredCharacterOverlay();
	void StopConfiguredCharacterOverlay();
	void StartConfiguredMissilePresentation();
	void UpdateConfiguredMissilePresentationTargets(const TArray<AActor*>& TargetActors);
	void StopConfiguredMissilePresentation();
	void StartConfiguredSelfBuff(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo);
	void StopConfiguredSelfBuff();
	FVector ResolveConfiguredCharacterDecalLocation(const ACharacterBase* Character) const;
	float ResolveConfiguredCharacterDecalDuration(const USkillDefinition* SkillDataAsset) const;
	void StartMovementContactDamage();
	void StopMovementContactDamage();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Activation")
	bool bAutoActivateWhenGranted = false;

private:
	friend class UAbilityMovementRuntime;
	friend class UAbilityPresentationRuntime;
	friend class UAbilityResourceRuntime;
	friend class UAbilitySourceRuntime;

	UPROPERTY(VisibleAnywhere, Instanced, Category = "!Ability|Runtime")
	TObjectPtr<UAbilityResourceRuntime> ResourceRuntime;

	UPROPERTY(VisibleAnywhere, Instanced, Category = "!Ability|Runtime")
	TObjectPtr<UAbilitySourceRuntime> SourceRuntime;

	UPROPERTY(VisibleAnywhere, Instanced, Category = "!Ability|Runtime")
	TObjectPtr<UAbilityMovementRuntime> MovementRuntime;

	UPROPERTY(VisibleAnywhere, Instanced, Category = "!Ability|Runtime")
	TObjectPtr<UAbilityPresentationRuntime> PresentationRuntime;
};
