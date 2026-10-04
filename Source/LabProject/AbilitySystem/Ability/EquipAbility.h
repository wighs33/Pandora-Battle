#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayTagContainer.h"
#include "EquipAbility.generated.h"

class UGameplayEffect;
class ACharacterBase;
class UAnimMontage;
class UItemDefinition;
class UAnimInstance;
class UCharacterActionDefinition;

UCLASS(Blueprintable)
class LABPROJECT_API UEquipAbility : public UPdGameplayAbility
{
	GENERATED_BODY()

protected:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	                             const FGameplayAbilityActivationInfo ActivationInfo,
	                             const FGameplayEventData* TriggerEventData) override;
	virtual const FGameplayTagContainer* GetCooldownTags() const override;
	virtual void ApplyCooldown(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo) const override;

public:
	// Public API ------------------------------------------------------------------------------------------------------
	UEquipAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	// Event Handlers --------------------------------------------------------------------------------------------------
	virtual void OnAbilityEnding() override;

	UFUNCTION()
	void OnEquipMontageCompleted();

	UFUNCTION()
	void OnEquipMontageInterrupted();

	UFUNCTION()
	void OnEquipMontageCancelled();

	UFUNCTION()
	void OnEquipCommitTiming(FGameplayEventData Payload);

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void ClearActiveEquipEffect();
	void ClearPendingEquipState();
	void ResolveEquipTransition();
	void FinalizeEquipCommit();
	bool CommitPendingEquipIfPossible();
	/** 장착 연출 큐를 캐릭터 위치에서 실행한다. 큐 태그가 없으면 하지 않는다. */
	void ExecuteEquipCue(ACharacterBase& Character, const UItemDefinition* ItemDefinition);
	/** 몽타주의 장착 확정 이벤트를 기다리며 장착 몽타주를 재생한다. 몽타주 작업을 만들지 못하면 false. */
	bool PlayEquipMontage(UAnimMontage* EquipMontage);

protected:
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

	UPROPERTY(Transient)
	bool bEquipTransitionResolved = false;

	UPROPERTY(Transient)
	bool bEquipAbilityCommitted = false;

	mutable FGameplayTagContainer EquipCooldownTags;

	/** 장착 쿨다운 길이를 정하는 기본 행동 정의. 처음 쿨다운을 걸 때 한 번 찾아 둔다. */
	mutable TWeakObjectPtr<const UCharacterActionDefinition> CachedActionDefinition;
};
