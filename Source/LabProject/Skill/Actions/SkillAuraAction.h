#pragma once

#include "ActiveGameplayEffectHandle.h"
#include "Skill/Actions/SkillAction.h"
#include "AbilitySystem/Ability/SkillAbility.h"
#include "Definition/AbilitySystem/SkillAuraSettings.h"
#include "Skill/Actions/SkillMovementContactDamage.h"
#include "TimerManager.h"
#include "SkillAuraAction.generated.h"

class ASkillEffectArea;
class ACharacterBase;
class USkillDefinition;
class UPrimitiveComponent;

/** 실행 중 장판 생성, 이동 속도 보정과 아군 회복을 관리한다. */
UCLASS(meta = (DisplayName = "Aura"))
class LABPROJECT_API USkillAuraAction : public USkillAction
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	USkillAuraAction();

protected:
	// Event Handlers --------------------------------------------------------------------------------------------------
	virtual void OnStart() override;

	virtual void OnStop() override;

private:
	void HandleRepeatedAuraEffectAreaSpawn();
	void HandleHealFieldTeamHealTick();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void StartAuraEffectAreaSpawning(const USkillDefinition* SkillDataAsset);
	void StopAuraEffectAreaSpawning();
	ACharacterBase* ResolveAuraSourceCharacter() const;
	void SpawnAuraEffectArea(const USkillDefinition* SkillDataAsset);
	void StartHealFieldTeamHealing(const USkillDefinition* SkillDataAsset);
	void StopHealFieldTeamHealing();
	void ApplyHealFieldTeamHeal(const USkillDefinition* SkillDataAsset);
	UPrimitiveComponent* FindInteractionHealComponent(ACharacterBase* Character, FName ComponentName) const;
	float ResolveHealFieldRadius(ACharacterBase* SourceCharacter, const USkillDefinition* SkillDataAsset) const;
	FVector ResolveHealFieldOrigin(ACharacterBase* SourceCharacter) const;
	bool IsCharacterInsideActiveHealField(const ACharacterBase* Character) const;
	bool ShouldHealInteractionTarget(const ACharacterBase* SourceCharacter, const ACharacterBase* TargetCharacter, const USkillDefinition* SkillDataAsset) const;
	FActiveGameplayEffectHandle ApplyTeamHealEffectToTarget(ACharacterBase* SourceCharacter, ACharacterBase* TargetCharacter, const USkillDefinition* SkillDataAsset) const;
	void RemoveInteractionHealEffectFromTarget(ACharacterBase* TargetCharacter, FActiveGameplayEffectHandle ActiveHandle) const;
	void ClearInteractionHealEffects();

public:
	/** 실행 중 장판 생성, 이동 속도 보정과 아군 회복을 관리한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings", meta = (ShowOnlyInnerProperties))
	FAuraSkillConfig Settings;

	/** 범위 안의 아군에게 적용할 회복 설정. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Healing", meta = (ShowOnlyInnerProperties))
	FSkillHealSettings Healing;

private:
	UPROPERTY(Transient)
	TObjectPtr<const USkillDefinition> ActiveAuraSkillDataAsset;

	UPROPERTY(Transient)
	TWeakObjectPtr<ACharacterBase> ActiveAuraSourceCharacter;

	FTimerHandle AuraEffectAreaSpawnTimerHandle;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<ASkillEffectArea>> ActiveAuraEffectAreas;

	FActiveGameplayEffectHandle MovementSpeedEffectHandle;

	/** 오라를 두른 채 움직이는 동안 닿은 적에게 피해를 준다. */
	FSkillMovementContactDamage ContactDamage;

	UPROPERTY(Transient)
	FVector ActiveHealFieldOrigin = FVector::ZeroVector;

	UPROPERTY(Transient)
	float ActiveHealFieldRadius = 0.0f;

	TMap<TWeakObjectPtr<ACharacterBase>, FActiveGameplayEffectHandle> ActiveInteractionHealEffectHandles;

	FTimerHandle HealFieldTeamHealTimerHandle;
};
