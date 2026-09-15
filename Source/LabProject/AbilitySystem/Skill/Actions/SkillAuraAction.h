#pragma once

#include "ActiveGameplayEffectHandle.h"
#include "AbilitySystem/Skill/SkillAction.h"
#include "AbilitySystem/Ability/SkillAbility.h"
#include "Definition/AbilitySystem/SkillAuraSettings.h"
#include "TimerManager.h"
#include "SkillAuraAction.generated.h"

class AEffectAreaBase;
class ACharacterBase;
class USkillDefinition;
class UPrimitiveComponent;

/** 실행 중 장판 생성, 이동 속도 보정과 아군 회복을 관리한다. */
UCLASS(meta = (DisplayName = "Aura"))
class LABPROJECT_API USkillAuraAction : public USkillAction
{
	GENERATED_BODY()

public:
	USkillAuraAction();

	/** 실행 중 장판 생성, 이동 속도 보정과 아군 회복을 관리한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings", meta = (ShowOnlyInnerProperties))
	FAuraSkillConfig Settings;

	/** 범위 안의 아군에게 적용할 회복 설정. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Healing", meta = (ShowOnlyInnerProperties))
	FSkillHealSettings Healing;

protected:
	virtual void OnStart() override;

	virtual void OnStop() override;

private:
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
	TObjectPtr<USkillDefinition> ActiveAuraSkillDataAsset;

	UPROPERTY(Transient)
	TWeakObjectPtr<ACharacterBase> ActiveAuraSourceCharacter;

	FTimerHandle AuraEffectAreaSpawnTimerHandle;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AEffectAreaBase>> ActiveAuraEffectAreas;

	FActiveGameplayEffectHandle MovementSpeedEffectHandle;

	UPROPERTY(Transient)
	FVector ActiveHealFieldOrigin = FVector::ZeroVector;

	UPROPERTY(Transient)
	float ActiveHealFieldRadius = 0.0f;

	TMap<TWeakObjectPtr<ACharacterBase>, FActiveGameplayEffectHandle> ActiveInteractionHealEffectHandles;

	FTimerHandle HealFieldTeamHealTimerHandle;
};
