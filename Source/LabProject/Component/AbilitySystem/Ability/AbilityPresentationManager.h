#pragma once

#include "CoreMinimal.h"
#include "Definition/AbilitySystem/SkillTypes.h"
#include "UObject/Object.h"
#include "AbilityPresentationManager.generated.h"

class ACharacterBase;
class ASkillPresentationActor;
class UCharacterPresentationComponent;
class USkillDefinition;
class UPdGameplayAbility;

/**
 * 한 능력의 시각효과 액터·데칼·캐릭터 확대 연출을 관리한다.
 *
 * 버프의 능력치와 GameplayEffect 적용·해제는 능력이 담당한다.
 */
UCLASS()
class LABPROJECT_API UAbilityPresentationManager : public UObject
{
	GENERATED_BODY()

public:
	void StartConfiguredDefaultFX(UPdGameplayAbility& Ability);
	void StopConfiguredDefaultFX(UPdGameplayAbility& Ability);
	void StartConfiguredGroundFX(UPdGameplayAbility& Ability);
	void StopConfiguredGroundFX(UPdGameplayAbility& Ability);
	void StartConfiguredCharacterOverlay(UPdGameplayAbility& Ability);
	void StopConfiguredCharacterOverlay(UPdGameplayAbility& Ability);
	void StartConfiguredMissilePresentation(UPdGameplayAbility& Ability);
	void UpdateConfiguredMissilePresentationTargets(const TArray<AActor*>& TargetActors);
	void StopConfiguredMissilePresentation(UPdGameplayAbility& Ability);
	void DestroyActiveSkillPresentationActor();

	void ApplySelfBuffCharacterScale(UPdGameplayAbility& Ability, const FSkillSelfBuffSettings& SelfBuffSettings);
	void RestoreSelfBuffCharacterScale(UPdGameplayAbility& Ability);

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

private:
	UPROPERTY(Transient)
	TObjectPtr<ASkillPresentationActor> ActiveSkillPresentationActor;

	TWeakObjectPtr<UCharacterPresentationComponent> SelfBuffScaleOwner;
};
