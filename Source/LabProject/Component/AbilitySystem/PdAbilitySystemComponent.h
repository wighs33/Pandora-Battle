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
class UPandoraSkillSource;

DECLARE_MULTICAST_DELEGATE(FPdAbilitiesChangedNativeDelegate);

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
	UPdAbilitySystemComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//------------------------------------------------------------------------------------------------------------------
	//--- Engine Callbacks
	virtual void ReadyForReplication() override;
	virtual void OnGiveAbility(FGameplayAbilitySpec& AbilitySpec) override;
	virtual void NotifyAbilityActivated(FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability) override;
	virtual void OnRemoveAbility(FGameplayAbilitySpec& AbilitySpec) override;
	virtual void NotifyAbilityEnded(FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability, bool bWasCancelled) override;
	virtual void NotifyAbilityFailed(FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability, const FGameplayTagContainer& FailureReason) override;
	virtual void ApplyAbilityBlockAndCancelTags(const FGameplayTagContainer& AbilityTags, UGameplayAbility* RequestingAbility,
		bool bEnableBlockTags, const FGameplayTagContainer& BlockTags, bool bExecuteCancelTags,
		const FGameplayTagContainer& CancelTags) override;

	//------------------------------------------------------------------------------------------------------------------

	void HandleAbilityInputPressed(const FGameplayTag& InputTag);
	void HandleAbilityInputReleased(const FGameplayTag& InputTag);
	void ProcessPendingInputReleases();

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
	void ReactivateAutoActivatedAbilities();

	void NotifyPandoraSourceReplicated(UPandoraSkillSource* Source);

	FPdAbilitiesChangedNativeDelegate OnAbilitiesChangedNative;


protected:
	virtual void OnRep_ActivateAbilities() override;
	virtual void ClientActivateAbilityFailed_Implementation(FGameplayAbilitySpecHandle Handle, int16 PredictionKey) override;
	virtual void ClientActivateAbilitySucceedWithEventData_Implementation(
		FGameplayAbilitySpecHandle Handle, FPredictionKey PredictionKey, FGameplayEventData TriggerEventData) override;
	virtual void ClientEndAbility_Implementation(FGameplayAbilitySpecHandle Handle, FGameplayAbilityActivationInfo ActivationInfo) override;
	virtual void ClientCancelAbility_Implementation(FGameplayAbilitySpecHandle Handle, FGameplayAbilityActivationInfo ActivationInfo) override;

private:
	void RegisterPandoraSkillSource(UObject* SourceObject);
	void ReleasePandoraSkillSourceIfUnused(UPandoraSkillSource* SkillSource, FGameplayAbilitySpecHandle RemovedHandle);
	void ActivateAbilitiesWithReadySources();
	void ClearPendingActivation(FGameplayAbilitySpecHandle Handle, const FPredictionKey& PredictionKey);
	void CancelActiveAbilitiesForDeath();
	int32 RemoveRuntimeEffects(const FGameplayTagContainer& EffectTags, const FGameplayTagContainer& OwnedTags,
		const FGameplayTagContainer& LooseTags, const FGameplayTagContainer& GameplayCues);


	UPROPERTY(Transient)
	TArray<TObjectPtr<UPandoraSkillSource>> GrantedPandoraSkillSources;

	UPROPERTY(VisibleAnywhere, Instanced, Category = "!AbilitySystem|Abilities")
	TObjectPtr<UAbilityGrantAndInputManager> AbilityGrantAndInputManager;

	// 서버의 활성화 통지가 스킬 출처보다 먼저 도착한 경우에만 실행을 보류한다.
	TArray<FPendingAbilityInfo> ActivationsWaitingForSource;
};
