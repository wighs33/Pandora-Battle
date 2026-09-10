#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "AbilityCostAndCooldownManager.generated.h"

class APawn;
class UGameplayEffect;
class USkillDefinition;
class UPdGameplayAbility;
class UAbilitySystemComponent;
class UPandoraSkillSource;
struct FGameplayAbilityActivationInfo;
struct FGameplayAbilityActorInfo;
struct FGameplayAbilitySpecHandle;
struct FGameplayAbilitySpec;
struct FGameplayEffectSpecHandle;

/** 한 능력의 마나·스태미나 비용과 출처별 쿨다운, 종료 시 적용할 쿨다운을 관리한다. */
UCLASS()
class LABPROJECT_API UAbilityCostAndCooldownManager : public UObject
{
	GENERATED_BODY()

public:
	// 능력과 직접 발사 경로가 공유하는 비용 규칙이다. 인스턴스 상태를 사용하거나 자원을 차감하지 않는다.
	static TSubclassOf<UGameplayEffect> GetCostGameplayEffectClass(const UObject* WorldContextObject);
	static float GetWeaponAttackStaminaCost(const APawn* AvatarPawn);
	static bool SetCostEffectMagnitudes(FGameplayEffectSpecHandle& SpecHandle, float ManaCost, float StaminaCost);

	// 비용 검사와 차감
	bool CheckCost(const UPdGameplayAbility& Ability, const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags) const;

	void ApplyCost(const UPdGameplayAbility& Ability, const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo& ActivationInfo) const;

	bool TryCommitAdditionalActionStaminaCost(const UPdGameplayAbility& Ability) const;

	// 스킬 설정과 출처에 따른 쿨다운
	const FGameplayTagContainer* BuildCooldownTags(
		const UPdGameplayAbility& Ability, const FGameplayTagContainer* ParentCooldownTags) const;

	bool CheckConfiguredCooldown(const UPdGameplayAbility& Ability, const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* ParentCooldownTags,
		FGameplayTagContainer* OptionalRelevantTags, bool& bOutHandled) const;

	bool ShouldDeferCooldown(
		const UPdGameplayAbility& Ability, const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo) const;

	bool ApplyConfiguredCooldownImmediately(const UPdGameplayAbility& Ability, const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo& ActivationInfo) const;

	static void GetPandoraCooldown(
		const UAbilitySystemComponent& ASC, const UPandoraSkillSource& Source, float& OutRemaining, float& OutDuration);

	// 종료 쿨다운 예약은 정상 종료 시 소비하거나, 강제 정리 시 명시적으로 비운다.
	void MarkCooldownForAbilityEnd() const { bApplySkillCooldownWhenAbilityEnds = true; }
	void ClearPendingAbilityEndCooldown() const { bApplySkillCooldownWhenAbilityEnds = false; }
	bool ConsumePendingCooldown(bool bAbilityWasCancelled = false);

private:
	static float GetDefaultActionStaminaCost(const UObject* WorldContextObject);
	static float CalculateAbilityStaminaCost(
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec* AbilitySpec, const USkillDefinition* SkillDataAsset);

	mutable FGameplayTagContainer CachedCooldownTags;
	mutable bool bApplySkillCooldownWhenAbilityEnds = false;
};
