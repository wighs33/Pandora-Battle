#pragma once

#include "ActiveGameplayEffectHandle.h"
#include "CoreMinimal.h"
#include "Definition/AbilitySystem/SkillTypes.h"
#include "UObject/Object.h"
#include "PdAbilityPresentationRuntime.generated.h"

class ACharacterBase;
class ASkillPresentationActor;
class AWeaponBase;
class USkeletalMeshComponent;
class USkillDefinition;
class UPdGameplayAbility;
struct FGameplayAbilityActivationInfo;
struct FGameplayAbilityActorInfo;
struct FGameplayAbilitySpecHandle;

/**
 * Owns replicated skill presentation, temporary self-buff visuals, and decals
 * for one ability instance.
 */
UCLASS()
class LABPROJECT_API UPdAbilityPresentationRuntime : public UObject
{
	GENERATED_BODY()

public:
	void StartConfiguredDefaultFX(UPdGameplayAbility& Ability);
	void StopConfiguredDefaultFX(UPdGameplayAbility& Ability);
	void StartConfiguredCharacterOverlay(UPdGameplayAbility& Ability);
	void StopConfiguredCharacterOverlay(UPdGameplayAbility& Ability);
	void StartConfiguredMissilePresentation(
		UPdGameplayAbility& Ability,
		const FVector& TargetLocation);
	void UpdateConfiguredMissilePresentationTarget(const FVector& TargetLocation);
	void StopConfiguredMissilePresentation(UPdGameplayAbility& Ability);
	void CleanupConfiguredPresentation();

	void StartConfiguredSelfBuff(
		UPdGameplayAbility& Ability,
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo& ActivationInfo);
	void StopConfiguredSelfBuff(UPdGameplayAbility& Ability);

	float CalculateSelfBuffMagnitude(
		const FSkillSelfBuffSettings& SelfBuffSettings) const;
	float CalculateSelfBuffWeaponDamageBonus(
		const FSkillSelfBuffSettings& SelfBuffSettings) const;
	void ApplySelfBuffCharacterScale(
		UPdGameplayAbility& Ability,
		const FSkillSelfBuffSettings& SelfBuffSettings);
	void RestoreSelfBuffCharacterScale();
	void ApplySelfBuffWeaponTraceEndZ(
		UPdGameplayAbility& Ability,
		const FSkillSelfBuffSettings& SelfBuffSettings);
	void RestoreSelfBuffWeaponTraceEndZ(UPdGameplayAbility& Ability);

	void SpawnConfiguredCharacterDecal(UPdGameplayAbility& Ability);
	FVector ResolveConfiguredCharacterDecalLocation(
		const ACharacterBase* Character) const;
	float ResolveConfiguredCharacterDecalDuration(
		const USkillDefinition* SkillDataAsset) const;

	ASkillPresentationActor* EnsureConfiguredPresentationActor(
		UPdGameplayAbility& Ability);
	void SetConfiguredPresentationEnabled(
		UPdGameplayAbility& Ability,
		uint8 PresentationFlag,
		bool bEnabled);

	ASkillPresentationActor* GetActivePresentationActor() const
	{
		return ActiveSkillPresentationActor.Get();
	}

private:
	UPROPERTY(Transient)
	TObjectPtr<ASkillPresentationActor> ActiveSkillPresentationActor;

	UPROPERTY(Transient)
	TArray<FActiveGameplayEffectHandle> ActiveSelfBuffEffectHandles;

	UPROPERTY(Transient)
	bool bTemporaryWeaponDamageBonusApplied = false;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> SelfBuffScaledMeshComponent;

	UPROPERTY(Transient)
	FVector CachedSelfBuffMeshWorldScale = FVector::OneVector;

	UPROPERTY(Transient)
	bool bSelfBuffCharacterScaleApplied = false;

	UPROPERTY(Transient)
	TObjectPtr<AWeaponBase> SelfBuffTraceEndZWeapon;

	UPROPERTY(Transient)
	bool bSelfBuffTraceEndZApplied = false;
};
