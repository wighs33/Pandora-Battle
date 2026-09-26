#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "Definition/AbilitySystem/SkillDefinition.h"
#include "Engine/OverlapResult.h"
#include "UObject/ObjectKey.h"
#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "Skill/Actions/SkillAction.h"
#include "SkillAbility.generated.h"

class UPandoraSkillSource;
class AWeaponBase;
class AMeleeWeapon;
class ASkillVisualActor;
class UCharacterPresentationComponent;
class UCombatComponent;
class UNiagaraSystem;
enum class ESkillPresentationFlags : uint8;
class USkillDefinition;

/**
 * SkillDefinition의 Action 트리를 실행하는 공통 스킬 Ability.
 *
 * 스킬별 입력, 지속시간, 비용·쿨다운과 Pandora 출처를 관리하고,
 * 실제 기능 실행은 SkillAction에 위임한다.
 */
UCLASS(Blueprintable)
class LABPROJECT_API USkillAbility : public UPdGameplayAbility
{
	GENERATED_BODY()

public:
	using Super::GetCooldownTimeRemaining;
	float GetCooldownTimeRemaining(const FGameplayAbilityActorInfo* ActorInfo) const override;
	void GetCooldownTimeRemainingAndDuration(
		FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, float& TimeRemaining, float& CooldownDuration) const override;

	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual bool CanActivateAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void InputReleased(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo) override;

protected:
	virtual bool CommitAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, FGameplayTagContainer* OptionalRelevantTags = nullptr) override;
	virtual const FGameplayTagContainer* GetCooldownTags() const override;
	virtual bool CheckCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void GetResourceCosts(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		float& ManaCost, float& StaminaCost) const override;

	virtual void PreActivate(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		FOnGameplayAbilityEnded::FDelegate* EndedDelegate,
		const FGameplayEventData* TriggerEventData = nullptr) override;

	// CommitAbility에서는 쿨다운을 시작하지 않고, 정상적인 스킬 종료 시 ApplyCooldownOnEnd에서 적용한다.
	virtual void ApplyCooldown(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo) const override;

public:
	// Public API ------------------------------------------------------------------------------------------------------
	USkillAbility(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool ShouldConfirmTargetingOnInputRelease() const override;
	virtual bool UsesInputRelease(const FGameplayAbilitySpec& Spec) const override;

	// SkillAction이 사용할 피해 EffectSpec을 현재 스킬의 피해 보정값으로 생성한다.
	FGameplayEffectSpecHandle MakeActionDamageSpec(
		const FSkillGameplayEffectConfig& Damage) const;

	// SkillAction이 사용할 상태 이상 EffectSpec을 현재 SkillDefinition으로 생성한다.
	FGameplayEffectSpecHandle MakeActionStatusSpec() const;

	// 현재 시전이 Action을 계속 실행할 수 있는 상태인지 확인한다.
	bool CanRunActions() const;

	// 같은 시전에서 여러 Action이 요청해도 비용과 사용 횟수를 한 번만 확정한다.
	bool CommitSkill();

	// 현재 시전이 Pandora에서 부여된 스킬인 경우에만 Source를 반환한다.
	const UPandoraSkillSource* GetPandoraSkillSource() const;

	// Duration 스킬은 활성화 시 확정한 하나의 종료 시점을 공유한다.
	bool HasDurationDeadline() const
	{
		return DurationEndTime >= 0.0;
	}

	// 준비·조준·몽타주 시간을 포함한 전체 지속시간 중 남은 시간을 반환한다.
	float GetRemainingDuration() const;

	USkillDefinition* GetSourceSkillDataAsset() const;
	AWeaponBase* GetCurrentWeaponActorFromAvatar() const;
	bool HasCurrentWeaponSkillTrail() const;
	bool StartCurrentWeaponSkillTrail(UNiagaraSystem* TrailSystem) const;
	void StopCurrentWeaponSkillTrail() const;
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
	void StartMovementContactDamage();
	void StartConfiguredDefaultFX();
	void StartConfiguredGroundFX();
	void StartConfiguredCharacterOverlay();
	void StartConfiguredMissilePresentation();
	void SetMissileTargeting(FName AimParameter, FName TargetSocket);
	void UpdateConfiguredMissilePresentationTargets(const TArray<AActor*>& TargetActors);
	void StopConfiguredMissilePresentation();
	void DestroyActiveSkillPresentationActor();
	void SpawnConfiguredCharacterDecal();
	FVector ResolveConfiguredCharacterDecalLocation(const ACharacterBase* Character) const;

	using UGameplayAbility::ApplyGameplayEffectSpecToOwner;
	using UGameplayAbility::BP_ApplyGameplayEffectToOwner;
	using UGameplayAbility::K2_AddGameplayCueWithParams;
	using UGameplayAbility::K2_ExecuteGameplayCueWithParams;
	using UGameplayAbility::K2_RemoveGameplayCue;
	using UGameplayAbility::MakeTargetLocationInfoFromOwnerActor;
	using UGameplayAbility::MakeTargetLocationInfoFromOwnerSkeletalMeshComponent;


protected:
	// Event Handlers --------------------------------------------------------------------------------------------------
	virtual void OnAbilityEnding() override;

private:
	void ActionFinished(USkillAction* Action, bool bSucceeded);
	void DurationFinished();

protected:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	virtual void ApplyCooldownOnEnd(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo) override;

	// 부모의 공통 피해 보정(Intelligence)에 현재 Pandora 슬롯 능력치를 추가한다.
	virtual float GetDamageBonusPercent() const override;

private:
	// Spec의 SourceObject에서 SkillDefinition을 찾는다.
	// 일반 스킬은 SkillDefinition을 직접 사용하고,
	// Pandora 스킬은 PandoraSkillSource를 통해 SkillDefinition을 가져온다.
	static const USkillDefinition* ResolveSkill(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo);

private:
	static const USkillDefinition* ResolveSourceSkillDataAsset(UObject* SourceObject);
	void FinishAbilityFromDuration();
	bool CanExecuteSkillPayload() const;
	void StartConfiguredSelfBuff(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo);
	void StopConfiguredSelfBuff();
	void StopAvatarMovementForSkillActivation();
	void StopDurationMovementLock();
	void StopMovementContactDamage();
	void HandleMovementContactDamageTick();
	void ApplyMovementContactDamageToActor(AActor* HitActor);
	ASkillVisualActor* GetOrCreatePresentationActor();
	void SetConfiguredPresentationEnabled(
		const ESkillPresentationFlags PresentationFlag, const bool bEnabled);
	void ApplySkillCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo& ActivationInfo) const;

	// 시전 도중 장비나 Avatar가 바뀌어도 처음 적용한 대상의 변경분만 복구한다.
	FActiveGameplayEffectHandle ActiveSelfBuffEffectHandle;
	TWeakObjectPtr<UAbilitySystemComponent> SelfBuffAbilitySystemComponent;
	TWeakObjectPtr<UCombatComponent> SelfBuffCombatComponent;
	TWeakObjectPtr<AMeleeWeapon> SelfBuffTraceEndZWeapon;
	TWeakObjectPtr<UCharacterPresentationComponent> SelfBuffScaleOwner;

	UPROPERTY(Transient)
	TObjectPtr<ASkillVisualActor> ActiveSkillPresentationActor;

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
	TArray<FHitResult> MovementContactSweepHits;
	TArray<FOverlapResult> MovementContactOverlapResults;

	UPROPERTY(Transient)
	TObjectPtr<USkillAction> ActiveAction;

	FTimerHandle DurationTimer;

	double ActivationTime = 0.0;
	double DurationEndTime = -1.0;

	// UsesPerCooldown이 2 이상일 때 여러 시전에 걸쳐 사용 횟수를 유지한다.
	int32 UsesSinceCooldown = 0;

	// 현재 시전에서 비용 처리가 완료되었는지 나타낸다.
	bool bSkillCommitted = false;
};