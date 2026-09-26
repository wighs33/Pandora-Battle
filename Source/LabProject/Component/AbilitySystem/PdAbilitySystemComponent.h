#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "Common/Enum_Operation.h"
#include "GameplayAbilitySpec.h"
#include "GameplayTagContainer.h"
#include "PdAbilitySystemComponent.generated.h"

class UGameplayAbility;
class UGameplayEffect;
class UStatUpgradeDefinition;
class UAbilityGrantAndInputManager;

DECLARE_MULTICAST_DELEGATE(FPdAbilitiesChangedNativeDelegate);
DECLARE_MULTICAST_DELEGATE_OneParam(FPdAbilityRemovedNativeDelegate, const FGameplayAbilitySpec&);

/**
 * 프로젝트의 능력 시스템을 GAS와 연결하는 컴포넌트.
 *
 * 속성 초기화·변경과 리셋은 직접 처리하고, 능력 목록과 입력 처리는 관리 객체에 위임하며,
 * 플레이어와 적이 동일한 공개 API를 사용한다.
 */
UCLASS(BlueprintType, Blueprintable, ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LABPROJECT_API UPdAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	// RemoveAbility로 능력 제거 직후
	virtual void OnRemoveAbility(FGameplayAbilitySpec& AbilitySpec) override;
	// EndAbility로 종료 시
	virtual void NotifyAbilityEnded(FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability, bool bWasCancelled) override;
	// TryActivateAbility 실패 시
	virtual void NotifyAbilityFailed(FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability, const FGameplayTagContainer& FailureReason) override;
	// Ability 활성화 시 Block/Cancel 태그를 적용할 때
	virtual void ApplyAbilityBlockAndCancelTags(const FGameplayTagContainer& AbilityTags, UGameplayAbility* RequestingAbility,
		bool bEnableBlockTags, const FGameplayTagContainer& BlockTags, bool bExecuteCancelTags,
		const FGameplayTagContainer& CancelTags) override;

protected:
	// 서버의 Ability 목록(ActivatableAbilities) 변경이 클라이언트에 복제되어 도착했을 때
	virtual void OnRep_ActivateAbilities() override;
	// LocalPredicted Ability를 클라이언트가 먼저 실행했는데 서버가 활성화를 거부했을 때
	virtual void ClientActivateAbilityFailed_Implementation(FGameplayAbilitySpecHandle Handle, int16 PredictionKey) override;

public:
	// Public API ------------------------------------------------------------------------------------------------------
	UPdAbilitySystemComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	const FGameplayAbilitySpec* FindActiveAbilitySpecByTags(const FGameplayTagContainer& AbilityTags) const;
	bool HasActiveAbilityWithTags(const FGameplayTagContainer& AbilityTags) const;
	bool HasActiveAbilityOfClass(TSubclassOf<UGameplayAbility> AbilityClass, bool bIncludeChildClasses = true) const;
	bool HasActiveAbilityOfAnyClass(const TArray<TSubclassOf<UGameplayAbility>>& AbilityClasses, bool bIncludeChildClasses = true) const;

	// 기본 능력치 초기화와 스탯 변경 (서버 전용)
	bool ApplyConfiguredAttributeDefaults(const UStatUpgradeDefinition& Definition);
	bool ApplyAttributeDefaultValue(const FGameplayAttribute& Attribute, float DefaultValue);
	bool ApplyStatUpEffectByTags(
		TSubclassOf<UGameplayEffect> GameplayEffectClass,
		const TMap<FGameplayTag, float>& StatMagnitudes,
		EEnum_Operation Operation = EEnum_Operation::Add,
		float Level = 1.0f);

	TArray<FGameplayAbilitySpecHandle> GrantAbilities(
		const TArray<TSubclassOf<UGameplayAbility>>& AbilityClasses,
		int32 AbilityLevel = 1);

	void RemoveAbilities(const TArray<FGameplayAbilitySpecHandle>& AbilityHandles);

	// 캐릭터의 사망 처리가 시작된 뒤 활성 능력과 사망 시 제거할 효과를 정리한다.
	void ResetAbilityRuntimeStateForDeath();
	int32 ClearStatusEffectsForRespawn();
	// 리스폰은 사망·상태이상·보호막까지 초기화하고, 자원 복구는 기존 효과를 유지한다.
	void ResetRuntimeStateForRespawn();
	void RestoreResourcesToMaximum();
	void ReactivateAutoActivatedAbilities();

	// Event Handlers --------------------------------------------------------------------------------------------------
	void HandleAbilityInputPressed(const FGameplayTag& InputTag);
	void HandleAbilityInputReleased(const FGameplayTag& InputTag);

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	void CancelActiveAbilitiesForDeath();
	int32 RemoveRuntimeEffects(const FGameplayTagContainer& EffectTags, const FGameplayTagContainer& OwnedTags,
		const FGameplayTagContainer& LooseTags, const FGameplayTagContainer& GameplayCues);

public:
	// 스킬바 구성 변경 완료 또는 복제 데이터 도착을 알린다. 개별 능력 부여에서는 방송하지 않는다.
	FPdAbilitiesChangedNativeDelegate OnAbilitiesChangedNative;
	// 종료 처리가 끝난 Spec을 알린다. 이 콜백 동안 Spec은 아직 능력 목록에 있을 수 있다.
	FPdAbilityRemovedNativeDelegate OnAbilityRemovedNative;

private:
	UPROPERTY(VisibleAnywhere, Instanced, Category = "!AbilitySystem|Abilities")
	TObjectPtr<UAbilityGrantAndInputManager> AbilityGrantAndInputManager;
};
