#pragma once

#include "CoreMinimal.h"
#include "Definition/AbilitySystem/SkillDefinition.h"
#include "UObject/Object.h"
#include "AbilityPresentationManager.generated.h"

enum class ESkillPresentationFlags : uint8;

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
	// Public API ------------------------------------------------------------------------------------------------------
	void StartConfiguredDefaultFX(UPdGameplayAbility& Ability);
	void StartConfiguredGroundFX(UPdGameplayAbility& Ability);
	void StartConfiguredCharacterOverlay(UPdGameplayAbility& Ability);
	void StartConfiguredMissilePresentation(UPdGameplayAbility& Ability);
	void SetMissileTargeting(FName AimParameter, FName TargetSocket);
	void UpdateConfiguredMissilePresentationTargets(const TArray<AActor*>& TargetActors);
	void StopConfiguredMissilePresentation(UPdGameplayAbility& Ability);
	void DestroyActiveSkillPresentationActor();

	void ApplySelfBuffCharacterScale(UPdGameplayAbility& Ability, const FSkillSelfBuffSettings& SelfBuffSettings);
	void RestoreSelfBuffCharacterScale(UPdGameplayAbility& Ability);

	void SpawnConfiguredCharacterDecal(UPdGameplayAbility& Ability);
	FVector ResolveConfiguredCharacterDecalLocation(const ACharacterBase* Character) const;

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	ASkillPresentationActor* GetOrCreatePresentationActor(UPdGameplayAbility& Ability);
	void SetConfiguredPresentationEnabled(UPdGameplayAbility& Ability, ESkillPresentationFlags PresentationFlag, bool bEnabled);

private:
	UPROPERTY(Transient)
	TObjectPtr<ASkillPresentationActor> ActiveSkillPresentationActor;

	TWeakObjectPtr<UCharacterPresentationComponent> SelfBuffScaleOwner;
};
