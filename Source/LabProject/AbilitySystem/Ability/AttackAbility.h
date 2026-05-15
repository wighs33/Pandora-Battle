#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "GameplayTagContainer.h"
#include "AttackAbility.generated.h"

class UGameplayEffect;
class AWeaponBase;

/**
 * <기본 공격 어빌리티>
 * - 공격 몽타주 재생과 연계 입력 처리를 담당합니다.
 * - 입력 가능 구간과 점프 섹션 타이밍을 이벤트 태그로 제어합니다.
 * - 공격 중 무기 오버랩 활성화와 공격 상태 정리를 함께 처리합니다.
 */
UCLASS(Blueprintable)
class LABPROJECT_API UAttackAbility : public UPdGameplayAbility
{
	GENERATED_BODY()

public:
	UAttackAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	const FGameplayTag& GetJumpSectionEventTag() const { return JumpSectionEventTag; }
	FName GetNextAttackSectionName() const;
	bool RequestJumpToSection(FName RequestedSectionName);

protected:
	// Timing hooks
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	// Delegate callbacks
	UFUNCTION()
	void OnAttackMontageCompleted();

	UFUNCTION()
	void OnAttackMontageInterrupted();

	UFUNCTION()
	void OnAttackMontageCancelled();

	UFUNCTION()
	void OnJumpSectionTiming(FGameplayEventData Payload);

	UFUNCTION()
	void OnAttackInputWindowOpened(FGameplayEventData Payload);

	UFUNCTION()
	void OnAttackInputWindowClosed(FGameplayEventData Payload);

	// State helpers
	void CleanupAttackState();
	void ResetAttackInputState();

	// Query helpers
	AWeaponBase* GetCurrentWeaponActor() const;
	FName GetCurrentAttackSectionName() const;
	bool IsAttackSectionNameValid(FName SectionName) const;

	// Action helpers
	void SetCurrentWeaponBeginOverlapEnabled(bool bEnabled) const;
	bool TryJumpToSection(FName SectionName);
	bool TryJumpToNextSection();

	// =================================================================================================================
	// === 공격 이벤트 태그
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Event", meta = (Categories = "GameplayEvent"))
	FGameplayTag AttackInputWindowStartEventTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Event", meta = (Categories = "GameplayEvent"))
	FGameplayTag AttackInputWindowEndEventTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Event", meta = (Categories = "GameplayEvent"))
	FGameplayTag JumpSectionEventTag;

	// =================================================================================================================
	// === 공격 상태 GE
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Effect")
	TSubclassOf<UGameplayEffect> AttackingEffectClass;

	// =================================================================================================================
	// === 런타임 입력 상태
	
	UPROPERTY(Transient)
	bool bCanReceiveAttackInput = false;

	UPROPERTY(Transient)
	bool bReachedJumpSectionTiming = false;

	UPROPERTY(Transient)
	FName BufferedJumpSectionName = NAME_None;
};
