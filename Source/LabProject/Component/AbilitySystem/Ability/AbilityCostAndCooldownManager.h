#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "AbilityCostAndCooldownManager.generated.h"

class APawn;
class UGameplayEffect;
class USkillDefinition;
class UPdGameplayAbility;
struct FGameplayAbilityActivationInfo;
struct FGameplayAbilityActorInfo;
struct FGameplayAbilitySpecHandle;
struct FGameplayAbilitySpec;
struct FGameplayEffectSpecHandle;

/** 한 능력의 비용 차감과 쿨다운 검사·적용을 담당한다. */
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

	// 스킬 출처별 쿨다운 검사와 최종 지속시간 적용
	bool CheckConfiguredCooldown(const UPdGameplayAbility& Ability, const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* ParentCooldownTags,
		FGameplayTagContainer* OptionalRelevantTags, bool& bOutHandled) const;

	bool ApplyConfiguredCooldown(const UPdGameplayAbility& Ability, const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo& ActivationInfo) const;

private:
	static float GetDefaultActionStaminaCost(const UObject* WorldContextObject);
	static float CalculateAbilityStaminaCost(
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec* AbilitySpec, const USkillDefinition* SkillDataAsset);
};
