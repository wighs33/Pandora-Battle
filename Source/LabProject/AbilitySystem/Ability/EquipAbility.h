#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayTagContainer.h"
#include "EquipAbility.generated.h"

class UGameplayEffect;
class UItemDefinition;
class UAnimInstance;

/**
 * < 장착 베이스 어빌리티 >
 *
 * - 아이템 데이터 애셋의 설정값에 맞춰서 커스텀된 장착이 실행됨
 */

UCLASS(Blueprintable)
class LABPROJECT_API UEquipAbility : public UPdGameplayAbility
{
	GENERATED_BODY()

public:
	UEquipAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	// Timing hooks
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	                             const FGameplayAbilityActivationInfo ActivationInfo,
	                             const FGameplayEventData* TriggerEventData) override;

	// Delegate callbacks
	UFUNCTION()
	void OnEquipMontageCompleted();

	UFUNCTION()
	void OnEquipMontageInterrupted();

	UFUNCTION()
	void OnEquipMontageCancelled();

	UFUNCTION()
	void OnEquipCommitTiming(FGameplayEventData Payload);

	// State helpers
	void ClearActiveEquipEffect();
	void ClearPendingEquipState();
	void FinalizeEquipCommit();
	bool CommitPendingEquipIfPossible();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Effect")
	TSubclassOf<UGameplayEffect> EquippedItemEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Effect")
	TSubclassOf<UGameplayEffect> EquipEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Cue", meta = (Categories = "GameplayCue"))
	FGameplayTag EquipCueTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Event", meta = (Categories = "GameplayEvent"))
	FGameplayTag CommitEquipEventTag;

	UPROPERTY(Transient)
	TObjectPtr<const UItemDefinition> ActiveEquipWeaponDefinition;

	UPROPERTY(Transient)
	TSubclassOf<UAnimInstance> PendingEquipAnimLayer;

	UPROPERTY(Transient)
	bool bEquipCommitted = false;
};
