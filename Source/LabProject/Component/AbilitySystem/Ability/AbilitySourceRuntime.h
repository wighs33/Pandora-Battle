#pragma once

#include "ActiveGameplayEffectHandle.h"
#include "CoreMinimal.h"
#include "Definition/AbilitySystem/SkillTypes.h"
#include "UObject/Object.h"
#include "AbilitySourceRuntime.generated.h"

class AWeaponBase;
class UGameplayEffect;
class UNiagaraSystem;
class UPandoraSkillRuntimeContext;
class UPdGameplayAbility;
class USkillDefinition;
struct FGameplayAbilityActorInfo;
struct FGameplayAbilitySpec;
struct FGameplayEffectSpecHandle;

/**
 * Resolves an ability's immutable skill source and builds effect data from it.
 *
 * The cached Pandora context is deliberately ability-instance-owned; the ASC
 * collection runtime still owns shared granted-spec bookkeeping.
 */
UCLASS()
class LABPROJECT_API UAbilitySourceRuntime : public UObject
{
	GENERATED_BODY()

public:
	const FGameplayAbilitySpec* ResolveCurrentAbilitySpec(const UPdGameplayAbility& Ability) const;
	UObject* GetCurrentAbilitySpecSourceObject(const UPdGameplayAbility& Ability) const;

	const USkillDefinition* ResolveSkillDataAsset(
		const UPdGameplayAbility& Ability,
		const FGameplayAbilitySpec* AbilitySpec,
		const FGameplayAbilityActorInfo* ActorInfo) const;

	USkillDefinition* GetSourceSkillDataAsset(const UPdGameplayAbility& Ability) const;
	UPandoraSkillRuntimeContext* GetSourceSkillRuntimeContext(const UPdGameplayAbility& Ability) const;
	UPandoraSkillRuntimeContext* ResolveSourceSkillRuntimeContextFromSelectedPandora(
		const UPdGameplayAbility& Ability) const;
	TArray<FProjectileImpactEffectAreaSpawnConfig> GetSourceProjectileImpactEffectAreas(
		const UPdGameplayAbility& Ability) const;

	int32 GetPandoraSkillIndex(const FGameplayAbilitySpec* AbilitySpec) const;
	bool IsPandoraSkillSpec(const FGameplayAbilitySpec* AbilitySpec) const;

	AWeaponBase* GetCurrentWeaponActorFromAvatar(const UPdGameplayAbility& Ability) const;
	bool HasCurrentWeaponSkillTrail(const UPdGameplayAbility& Ability) const;
	bool StartCurrentWeaponSkillTrail(
		const UPdGameplayAbility& Ability,
		UNiagaraSystem* TrailSystem) const;
	void StopCurrentWeaponSkillTrail(const UPdGameplayAbility& Ability) const;

	float GetPandoraAttackDamageBonus(const UPdGameplayAbility& Ability) const;
	float GetPandoraLoadoutAttackDamageBonus(const UPdGameplayAbility& Ability) const;
	float CalculateBaseSkillDamageMagnitude(
		const FSkillGameplayEffectConfig& DamageConfig) const;
	float ApplyIntelligenceToSkillDamage(
		const UPdGameplayAbility& Ability,
		float DamageMagnitude) const;
	float CalculateSkillDamageMagnitude(
		const UPdGameplayAbility& Ability,
		const FSkillGameplayEffectConfig& DamageConfig) const;
	FGameplayEffectSpecHandle MakeConfiguredDamageEffectSpec(
		const UPdGameplayAbility& Ability,
		const FSkillGameplayEffectConfig& DamageConfig,
		float DamageMagnitude,
		UObject* SourceObject) const;

private:
	UPROPERTY(Transient)
	TObjectPtr<UPandoraSkillRuntimeContext> CachedResolvedSourceSkillRuntimeContext;
};
