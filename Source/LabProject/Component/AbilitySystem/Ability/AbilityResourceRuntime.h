#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "AbilityResourceRuntime.generated.h"

class UPdGameplayAbility;
class UAbilitySystemComponent;
class UPandoraSkillSource;
struct FGameplayAbilityActivationInfo;
struct FGameplayAbilityActorInfo;
struct FGameplayAbilitySpecHandle;
struct FGameplayEffectSpecHandle;

/**
 * Owns the per-ability resource and cooldown runtime state.
 *
 * GAS lifecycle overrides remain on UPdGameplayAbility, while project-specific
 * mana, stamina, source-specific cooldown, and deferred cooldown behavior lives here.
 */
UCLASS()
class LABPROJECT_API UAbilityResourceRuntime : public UObject
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

	void AppendCooldownRemovalPolicyTags(
		FGameplayEffectSpecHandle& CooldownSpecHandle,
		const FGameplayTagContainer& RemovalPolicyTags) const;

	bool TryCommitAdditionalActionStaminaCost(const UPdGameplayAbility& Ability) const;

	static void GetPandoraCooldown(const UAbilitySystemComponent& ASC, const UPandoraSkillSource& Source,
		float& OutRemaining, float& OutDuration);
	float GetSkillCooldownReductionPercent(const UPdGameplayAbility& Ability) const;

	void MarkCooldownForAbilityEnd() const { bApplySkillCooldownWhenAbilityEnds = true; }
	void SuppressPendingCooldown() const { bApplySkillCooldownWhenAbilityEnds = false; }
	bool ConsumePendingCooldown(bool bAbilityWasCancelled = false);

private:
	mutable FGameplayTagContainer RuntimeCooldownTags;
	mutable bool bApplySkillCooldownWhenAbilityEnds = false;
};
