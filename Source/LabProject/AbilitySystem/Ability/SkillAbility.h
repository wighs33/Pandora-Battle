#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "AbilitySystem/Skill/SkillAction.h"
#include "SkillAbility.generated.h"

/** 한 번의 비용/쿨다운과 취소 경계 안에서 기능 트리를 실행한다. */
UCLASS(Blueprintable)
class LABPROJECT_API USkillAbility : public UPdGameplayAbility
{
	GENERATED_BODY()
public:
	USkillAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual bool ShouldConfirmTargetingOnInputRelease() const override;
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void InputReleased(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo) override;

	FGameplayEffectSpecHandle MakeActionDamageSpec(const FSkillGameplayEffectConfig& Damage) const;
	FGameplayEffectSpecHandle MakeActionStatusSpec() const;
	bool CanRunActions() const;
	/** Duration 스킬은 활성화 시 확정한 하나의 종료 시점을 공유한다. */
	bool HasDurationDeadline() const { return DurationEndTime >= 0.0; }
	/** 준비 동작을 포함한 전체 지속시간 중 남은 시간. 다른 종료 정책은 0을 반환한다. */
	float GetRemainingDuration() const;
	/** 같은 시전의 여러 액션이 요청해도 비용과 쿨다운은 한 번만 확정한다. */
	bool CommitSkill();

	// 액션은 GAS 상태를 소유하지 않고 이 실행자의 공통 기능을 사용한다.
	using UGameplayAbility::ApplyGameplayEffectSpecToOwner;
	using UGameplayAbility::BP_ApplyGameplayEffectToOwner;
	using UGameplayAbility::K2_AddGameplayCueWithParams;
	using UGameplayAbility::K2_ExecuteGameplayCueWithParams;
	using UGameplayAbility::K2_RemoveGameplayCue;
	using UGameplayAbility::MakeTargetLocationInfoFromOwnerActor;
	using UGameplayAbility::MakeTargetLocationInfoFromOwnerSkeletalMeshComponent;
	using UPdGameplayAbility::ApplyConfiguredStatusEffectToTarget;
	using UPdGameplayAbility::ApplyIntelligenceToSkillDamage;
	using UPdGameplayAbility::CalculateBaseSkillDamageMagnitude;
	using UPdGameplayAbility::CalculateSkillDamageMagnitude;
	using UPdGameplayAbility::MakeConfiguredDamageEffectSpec;
	using UPdGameplayAbility::MakeConfiguredStatusEffectSpec;
	using UPdGameplayAbility::CreateDefaultMontageAndWaitTask;
	using UPdGameplayAbility::CreateWaitGameplayEventTask;
	using UPdGameplayAbility::BeginSpawningTargetDataActor;
	using UPdGameplayAbility::FinishSpawningTargetDataActor;
	using UPdGameplayAbility::GetPresentationManager;
	using UPdGameplayAbility::GetCurrentWeaponActorFromAvatar;
	using UPdGameplayAbility::HasCurrentWeaponSkillTrail;
	using UPdGameplayAbility::StartCurrentWeaponSkillTrail;
	using UPdGameplayAbility::StopCurrentWeaponSkillTrail;
	using UPdGameplayAbility::LockAvatarMovementForAbility;
	using UPdGameplayAbility::RestoreAvatarMovementForAbility;
	using UPdGameplayAbility::StartDurationMovementLock;
	using UPdGameplayAbility::StartMovementContactDamage;

protected:
	virtual void PreActivate(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, FOnGameplayAbilityEnded::FDelegate* EndedDelegate,
		const FGameplayEventData* TriggerEventData = nullptr) override;
	virtual void OnAbilityEnding() override;
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;
	virtual void ApplyCooldownOnEnd(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override;

private:
	void ActionFinished(USkillAction* Action, bool bSucceeded);
	void DurationFinished();

	UPROPERTY(Transient)
	TObjectPtr<USkillAction> ActiveAction;

	FTimerHandle DurationTimer;
	double ActivationTime = 0.0;
	double DurationEndTime = -1.0;
	int32 UsesSinceCooldown = 0;
	bool bSkillCommitted = false;
};
