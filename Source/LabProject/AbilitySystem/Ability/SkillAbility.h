#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "AbilitySystem/Skill/SkillAction.h"
#include "SkillAbility.generated.h"

class UPandoraSkillSource;
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
	USkillAbility(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//------------------------------------------------------------------------------------------------------------------
	//--- Ability Lifecycle

	virtual bool CanActivateAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual bool ShouldConfirmTargetingOnInputRelease() const override;

	virtual void ActivateAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void InputReleased(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo) override;

	//------------------------------------------------------------------------------------------------------------------
	//--- Skill Execution

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

	//------------------------------------------------------------------------------------------------------------------
	//--- Duration

	// Duration 스킬은 활성화 시 확정한 하나의 종료 시점을 공유한다.
	bool HasDurationDeadline() const
	{
		return DurationEndTime >= 0.0;
	}

	// 준비·조준·몽타주 시간을 포함한 전체 지속시간 중 남은 시간을 반환한다.
	float GetRemainingDuration() const;

	//------------------------------------------------------------------------------------------------------------------
	//--- APIs exposed to SkillAction

	using UGameplayAbility::ApplyGameplayEffectSpecToOwner;
	using UGameplayAbility::BP_ApplyGameplayEffectToOwner;
	using UGameplayAbility::K2_AddGameplayCueWithParams;
	using UGameplayAbility::K2_ExecuteGameplayCueWithParams;
	using UGameplayAbility::K2_RemoveGameplayCue;
	using UGameplayAbility::MakeTargetLocationInfoFromOwnerActor;
	using UGameplayAbility::MakeTargetLocationInfoFromOwnerSkeletalMeshComponent;

	using UPdGameplayAbility::ApplyConfiguredStatusEffectToTarget;
	using UPdGameplayAbility::BeginSpawningTargetDataActor;
	using UPdGameplayAbility::CreateDefaultMontageAndWaitTask;
	using UPdGameplayAbility::CreateWaitGameplayEventTask;
	using UPdGameplayAbility::FinishSpawningTargetDataActor;
	using UPdGameplayAbility::GetCurrentWeaponActorFromAvatar;
	using UPdGameplayAbility::GetPresentationManager;
	using UPdGameplayAbility::HasCurrentWeaponSkillTrail;
	using UPdGameplayAbility::LockAvatarMovementForAbility;
	using UPdGameplayAbility::MakeConfiguredDamageEffectSpec;
	using UPdGameplayAbility::MakeConfiguredStatusEffectSpec;
	using UPdGameplayAbility::RestoreAvatarMovementForAbility;
	using UPdGameplayAbility::StartCurrentWeaponSkillTrail;
	using UPdGameplayAbility::StartDurationMovementLock;
	using UPdGameplayAbility::StartMovementContactDamage;
	using UPdGameplayAbility::StopCurrentWeaponSkillTrail;

protected:
	//------------------------------------------------------------------------------------------------------------------
	//--- Ability Lifecycle

	virtual void PreActivate(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		FOnGameplayAbilityEnded::FDelegate* EndedDelegate,
		const FGameplayEventData* TriggerEventData = nullptr) override;

	virtual void OnAbilityEnding() override;

	//------------------------------------------------------------------------------------------------------------------
	//--- Cooldown

	// CommitAbility에서는 쿨다운을 시작하지 않고, 정상적인 스킬 종료 시 ApplyCooldownOnEnd에서 적용한다.
	virtual void ApplyCooldown(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo) const override;

	virtual void ApplyCooldownOnEnd(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo) override;

	//------------------------------------------------------------------------------------------------------------------
	//--- Damage

	// 부모의 공통 피해 보정(Intelligence)에 현재 Pandora 슬롯 능력치를 추가한다.
	virtual float GetDamageBonusPercent() const override;

private:
	//------------------------------------------------------------------------------------------------------------------
	//--- Skill Source

	// Spec의 SourceObject에서 SkillDefinition을 찾는다.
	// 일반 스킬은 SkillDefinition을 직접 사용하고,
	// Pandora 스킬은 PandoraSkillSource를 통해 SkillDefinition을 가져온다.
	static const USkillDefinition* ResolveSkill(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo);

	//------------------------------------------------------------------------------------------------------------------
	//--- Action Callbacks

	void ActionFinished(USkillAction* Action, bool bSucceeded);
	void DurationFinished();

	//------------------------------------------------------------------------------------------------------------------
	//--- Runtime State

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