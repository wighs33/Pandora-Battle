#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayTagContainer.h"
#include "UnequipAbility.generated.h"

class UGameplayEffect;
class UItemDefinition;

/**
 * <장착 해제 베이스 어빌리티>
 * - 장착 해제 몽타주와 장착 해제 시점을 처리합니다.
 * - 장착 중 효과 제거와 애니메이션 복구를 담당합니다.
 * - 아이템 태그 기준으로 장착 완료 효과를 제거합니다.
 */
UCLASS(Blueprintable)
class LABPROJECT_API UUnequipAbility : public UPdGameplayAbility
{
	GENERATED_BODY()

public:
	UUnequipAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	// Timing hooks
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	// Delegate callbacks
	UFUNCTION()
	void OnUnequipMontageCompleted();

	UFUNCTION()
	void OnUnequipMontageInterrupted();

	UFUNCTION()
	void OnUnequipMontageCancelled();

	UFUNCTION()
	void OnUnequipCommitTiming(FGameplayEventData Payload);

	// State helpers
	void ClearActiveUnequipEffect();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Effect")
	TSubclassOf<UGameplayEffect> UnequipEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Event", meta = (Categories = "GameplayEvent"))
	FGameplayTag CommitUnequipEventTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Activation", meta = (Categories = "Action"))
	FGameplayTag PostUnequipEquipAbilityTag;

	UPROPERTY(Transient)
	TObjectPtr<const UItemDefinition> ActiveUnequipWeaponDefinition;
};
