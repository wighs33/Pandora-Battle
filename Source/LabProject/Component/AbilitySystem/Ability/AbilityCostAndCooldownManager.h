#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "AbilityCostAndCooldownManager.generated.h"

class UPdGameplayAbility;
class UAbilitySystemComponent;
class UPandoraSkillSource;
struct FGameplayAbilityActivationInfo;
struct FGameplayAbilityActorInfo;
struct FGameplayAbilitySpecHandle;
struct FGameplayEffectSpecHandle;

/** 한 능력의 마나·스태미나 비용과 출처별 쿨다운, 종료 시 적용할 쿨다운을 관리한다. */
UCLASS()
class LABPROJECT_API UAbilityCostAndCooldownManager : public UObject
{
	GENERATED_BODY()

public:
	const FGameplayTagContainer* BuildCooldownTags(
		const UPdGameplayAbility& Ability,
		const FGameplayTagContainer* ParentCooldownTags) const;

	bool CheckCost(
		const UPdGameplayAbility& Ability,
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags) const;

	void ApplyCost(
		const UPdGameplayAbility& Ability,
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo& ActivationInfo) const;

	bool CheckConfiguredCooldown(
		const UPdGameplayAbility& Ability,
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* ParentCooldownTags,
		FGameplayTagContainer* OptionalRelevantTags,
		bool& bOutHandled) const;

	bool ShouldDeferCooldown(
		const UPdGameplayAbility& Ability,
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo) const;

	bool ApplyConfiguredCooldownImmediately(
		const UPdGameplayAbility& Ability,
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo& ActivationInfo) const;

	bool TryCommitAdditionalActionStaminaCost(const UPdGameplayAbility& Ability) const;

	static void GetPandoraCooldown(const UAbilitySystemComponent& ASC, const UPandoraSkillSource& Source,
		float& OutRemaining, float& OutDuration);
	float GetSkillCooldownReductionPercent(const UPdGameplayAbility& Ability) const;

	void MarkCooldownForAbilityEnd() const { bApplySkillCooldownWhenAbilityEnds = true; }
	void SuppressPendingCooldown() const { bApplySkillCooldownWhenAbilityEnds = false; }
	bool ConsumePendingCooldown(bool bAbilityWasCancelled = false);

private:
	mutable FGameplayTagContainer CachedCooldownTags;
	mutable bool bApplySkillCooldownWhenAbilityEnds = false;
};
