#pragma once

#include "CoreMinimal.h"
#include "Ability/PdGameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayTagContainer.h"
#include "EquipAbility.generated.h"

class UGameplayEffect;
class UItemDefinition;

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
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	                             const FGameplayAbilityActivationInfo ActivationInfo,
	                             const FGameplayEventData* TriggerEventData) override;

	UFUNCTION()
	void HandleEquipMontageCompleted();

	UFUNCTION()
	void HandleEquipMontageInterrupted();

	UFUNCTION()
	void HandleEquipMontageCancelled();

	UFUNCTION()
	void HandleEquipCommitEvent(FGameplayEventData Payload);

	void ClearActiveEquipEffect();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Effect")
	TSubclassOf<UGameplayEffect> EquippedItemEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Event", meta = (Categories = "GameplayEvent"))
	FGameplayTag CommitEquipEventTag;

	UPROPERTY(Transient)
	FActiveGameplayEffectHandle ActiveEquipEffectHandle;

	UPROPERTY(Transient)
	TObjectPtr<const UItemDefinition> ActiveEquipItemDefinition;
};
