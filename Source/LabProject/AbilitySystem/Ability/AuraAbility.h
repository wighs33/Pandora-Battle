#pragma once

#include "ActiveGameplayEffectHandle.h"
#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "TimerManager.h"
#include "AuraAbility.generated.h"

class UAbilityTask_WaitDelay;
class AEffectAreaBase;
class ACharacterBase;
class USkillDefinition;
class UPrimitiveComponent;

UCLASS(Blueprintable)
class LABPROJECT_API UAuraAbility : public UPdGameplayAbility
{
	GENERATED_BODY()

public:
	UAuraAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	virtual void InputReleased(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo) override;

private:
	UFUNCTION()
	void OnAuraDurationFinished();

	UFUNCTION()
	void OnAuraPressMinimumDurationFinished();

	void StartAuraEffectAreaSpawning(USkillDefinition* SkillDataAsset);
	void StopAuraEffectAreaSpawning();
	ACharacterBase* ResolveAuraSourceCharacter() const;
	void SpawnAuraEffectArea(const USkillDefinition* SkillDataAsset, const TCHAR* SpawnReason);
	void HandleRepeatedAuraEffectAreaSpawn();
	void ApplyMovementSpeedIncrease(const USkillDefinition* SkillDataAsset);
	void RemoveMovementSpeedIncrease();
	void StartHealFieldTeamHealing(USkillDefinition* SkillDataAsset);
	void StopHealFieldTeamHealing();
	void HandleHealFieldTeamHealTick();
	void ApplyHealFieldTeamHeal(const USkillDefinition* SkillDataAsset, const TCHAR* HealReason);
	UPrimitiveComponent* FindInteractionHealComponent(ACharacterBase* Character, FName ComponentName) const;
	float ResolveHealFieldRadius(ACharacterBase* SourceCharacter, const USkillDefinition* SkillDataAsset) const;
	FVector ResolveHealFieldOrigin(ACharacterBase* SourceCharacter) const;
	bool IsCharacterInsideActiveHealField(const ACharacterBase* Character) const;
	bool ShouldHealInteractionTarget(const ACharacterBase* SourceCharacter, const ACharacterBase* TargetCharacter, const USkillDefinition* SkillDataAsset) const;
	FActiveGameplayEffectHandle ApplyTeamHealEffectToTarget(ACharacterBase* SourceCharacter, ACharacterBase* TargetCharacter, const USkillDefinition* SkillDataAsset, const TCHAR* HealReason) const;
	void RemoveInteractionHealEffectFromTarget(ACharacterBase* TargetCharacter, FActiveGameplayEffectHandle ActiveHandle, const TCHAR* RemoveReason) const;
	void ClearInteractionHealEffects(const TCHAR* RemoveReason);

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitDelay> AuraDurationTask;

	UPROPERTY(Transient)
	TObjectPtr<USkillDefinition> ActiveAuraSkillDataAsset;

	UPROPERTY(Transient)
	TWeakObjectPtr<ACharacterBase> ActiveAuraSourceCharacter;

	FTimerHandle AuraEffectAreaSpawnTimerHandle;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AEffectAreaBase>> ActiveAuraEffectAreas;

	FActiveGameplayEffectHandle MovementSpeedEffectHandle;

	double AuraActivationWorldTime = 0.0;

	UPROPERTY(Transient)
	FVector ActiveHealFieldOrigin = FVector::ZeroVector;

	UPROPERTY(Transient)
	float ActiveHealFieldRadius = 0.0f;

	TMap<TWeakObjectPtr<ACharacterBase>, FActiveGameplayEffectHandle> ActiveInteractionHealEffectHandles;

	FTimerHandle HealFieldTeamHealTimerHandle;
};
