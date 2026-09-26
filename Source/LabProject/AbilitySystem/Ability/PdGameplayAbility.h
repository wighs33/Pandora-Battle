#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Abilities/GameplayAbility.h"
#include "PdGameplayAbility.generated.h"

class ACharacterBase;
class AGameplayAbilityTargetActor;
class APawn;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitTargetData;
class UAbilityTask_WaitGameplayEvent;
class UAnimMontage;
class UGameplayEffect;
class UPdAbilitySystemComponent;
struct FSkillGameplayEffectConfig;

/** GAS 공통 종료·입력 정책과 능력의 자원 비용, 효과·태스크 실행 도구를 제공한다. */
UCLASS(Abstract, Blueprintable)
class LABPROJECT_API UPdGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

protected:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override final;
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;

public:
	// Public API ------------------------------------------------------------------------------------------------------
	UPdGameplayAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	ACharacterBase* GetPdCharacterFromActorInfo() const;
	UPdAbilitySystemComponent* GetPdAbilitySystemComponentFromActorInfo() const;
	AActor* GetAttackTargetFromAvatar() const;
	bool HasPlayerController() const;
	bool ShouldAutoActivateWhenGranted() const { return bAutoActivateWhenGranted; }
	virtual bool ShouldConfirmTargetingOnInputRelease() const;
	virtual bool UsesInputRelease(const FGameplayAbilitySpec& Spec) const;
	virtual FGameplayTag GetDefaultInputTag() const { return FGameplayTag(); }

	bool ApplyGameplayEffect(TSubclassOf<UGameplayEffect> GameplayEffectClass, float EffectLevel = 1.0f, int32 StackCount = 1);
	bool RemoveGameplayEffect(TSubclassOf<UGameplayEffect> GameplayEffectClass);
	int32 RemoveGameplayEffectsWithGrantedTags(const FGameplayTagContainer& GrantedTags);
	// 스킬과 반응형 상태이상 능력이 공유하는 지능 기반 피해 계산이다.
	float CalculateDamageMagnitude(const FSkillGameplayEffectConfig& DamageConfig) const;

	// 능력과 원거리 직접 공격 경로가 같은 비용 효과와 SetByCaller 규칙을 사용한다.
	static TSubclassOf<UGameplayEffect> GetCostGameplayEffectClass(const UObject* WorldContextObject);
	static float GetWeaponAttackStaminaCost(const APawn* AvatarPawn);
	static bool SetCostEffectMagnitudes(FGameplayEffectSpecHandle& SpecHandle, float ManaCost, float StaminaCost);

	UAbilityTask_PlayMontageAndWait* CreateDefaultMontageAndWaitTask(UAnimMontage* MontageToPlay);
	UAbilityTask_WaitGameplayEvent* CreateWaitGameplayEventTask(
		const FGameplayTag& EventTag, bool bOnlyTriggerOnce = false, bool bOnlyMatchExact = true);
	AGameplayAbilityTargetActor* BeginSpawningTargetDataActor(
		UAbilityTask_WaitTargetData* TargetDataTask, TSubclassOf<AGameplayAbilityTargetActor> TargetActorClass);
	void FinishSpawningTargetDataActor(UAbilityTask_WaitTargetData* TargetDataTask, AGameplayAbilityTargetActor* SpawnedActor);

protected:
	// Event Handlers --------------------------------------------------------------------------------------------------
	// 종료 유효성·잠금·재진입 검사 후 호출한다. GAS 종료 함수는 공통 기반에서 관리한다.
	virtual void OnAbilityEnding();
	virtual void OnAbilityEnded(bool bWasCancelled);

	// Internal Helpers ------------------------------------------------------------------------------------------------
	// 정상 종료 시점의 정책 경계다. 파생 능력이 사망 여부 등 추가 조건을 검사한다.
	virtual void ApplyCooldownOnEnd(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo);
	virtual float GetDamageBonusPercent() const;
	virtual void GetResourceCosts(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		float& ManaCost, float& StaminaCost) const;
	static float GetDefaultActionStaminaCost(const UObject* WorldContextObject);
	bool HasActiveGameplayEffect(TSubclassOf<UGameplayEffect> GameplayEffectClass) const;
	bool ApplySharedCooldownEffect(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, float CooldownDuration, const FGameplayTagContainer& CooldownTags) const;
	bool TryCommitAdditionalActionStaminaCost() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Cooldown|Policy", meta = (Categories = "Effect.Policy"))
	FGameplayTagContainer CooldownRemovalPolicyTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Activation")
	bool bAutoActivateWhenGranted = false;

private:
	// 파생 능력 정리 중 들어오는 종료 요청의 재진입을 막는다.
	bool bIsCleaningUpAbility = false;
};
