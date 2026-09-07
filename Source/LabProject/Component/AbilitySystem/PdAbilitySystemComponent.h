#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "Common/Enum_Operation.h"
#include "Definition/AbilitySystem/AbilityAttributeConfig.h"
#include "GameplayAbilitySpec.h"
#include "GameplayTagContainer.h"
#include "PdAbilitySystemComponent.generated.h"

class UGameplayAbility;
class UGameplayEffect;
class UAttributeSet;
class UStatUpgradeDefinition;
class UAbilityAttributeRuntime;
class UAbilityCollectionRuntime;
class UPandoraSkillRuntimeContext;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPdAbilitiesChangedDynamicDelegate);
DECLARE_MULTICAST_DELEGATE(FPdAbilitiesChangedNativeDelegate);

/**
 * 프로젝트의 능력 시스템을 GAS와 연결하는 컴포넌트.
 *
 * 속성과 능력 목록은 내부 Runtime에 위임하고, 리셋은 ASC에서 처리하며,
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
	virtual void OnRemoveAbility(FGameplayAbilitySpec& AbilitySpec) override;
	virtual void NotifyAbilityActivated(FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability) override;

	//------------------------------------------------------------------------------------------------------------------

	// ActorInfo의 할당 여부이며, Owner와 Avatar의 초기화 완료를 의미하지는 않는다.
	bool HasAbilityActorInfoAllocated() const { return AbilityActorInfo.IsValid(); }

	int32 AddAttributeConfig(const FAttributeConfig& AttributeConfig);
	void RemoveAttributeConfig(int32 AttributeConfigHandle);
	bool ApplyConfiguredAttributeDefaults(const UStatUpgradeDefinition& Definition);
	bool ApplyAttributeDefaultValue(const FGameplayAttribute& Attribute, float DefaultValue);
	bool HasAppliedConfiguredAttributeDefaults(const UAttributeSet* AttributeSet, const FSoftObjectPath& DefinitionPath) const;
	void MarkConfiguredAttributeDefaultsApplied(UAttributeSet* AttributeSet, const FSoftObjectPath& DefinitionPath);
	void ClearConfiguredAttributeDefaultsApplied(const UAttributeSet* AttributeSet, const FSoftObjectPath& DefinitionPath);

	void AbilityInputTagPressed(const FGameplayTag& InputTag);
	void AbilityInputTagReleased(const FGameplayTag& InputTag);

	const FGameplayAbilitySpec* FindActiveAbilitySpecByTags(const FGameplayTagContainer& AbilityTags) const;
	bool HasActiveAbilityWithTags(const FGameplayTagContainer& AbilityTags) const;
	bool HasActiveAbilityOfClass(TSubclassOf<UGameplayAbility> AbilityClass, bool bIncludeChildClasses = true) const;
	bool HasActiveAbilityOfAnyClass(const TArray<TSubclassOf<UGameplayAbility>>& AbilityClasses, bool bIncludeChildClasses = true) const;

	bool ApplyStatUpEffectByTag(
		TSubclassOf<UGameplayEffect> GameplayEffectClass,
		FGameplayTag StatTag,
		float Magnitude,
		EEnum_Operation Operation = EEnum_Operation::Add,
		float Level = 1.0f);

	bool ApplyStatUpEffectByTags(
		TSubclassOf<UGameplayEffect> GameplayEffectClass,
		const TMap<FGameplayTag, float>& StatMagnitudes,
		EEnum_Operation Operation = EEnum_Operation::Add,
		float Level = 1.0f);

	TArray<FGameplayAbilitySpecHandle> GrantAbilities(
		const TArray<TSubclassOf<UGameplayAbility>>& AbilityClasses,
		int32 AbilityLevel = 1,
		UObject* SourceObject = nullptr);

	void RemoveAbilities(const TArray<FGameplayAbilitySpecHandle>& AbilityHandles);

	void ResetAbilityRuntimeStateForDeath();
	int32 ClearStatusEffectsForRespawn();
	void ReactivateAutoActivatedAbilities();
	bool IsResettingAbilityRuntimeState() const { return bResettingAbilityRuntimeState; }

	bool ResolveAttributeFromTag(const FGameplayTag& StatTag, FGameplayAttribute& OutAttribute) const;
	bool ResolveDamageMagnitudeSetByCallerTag(FGameplayTag& OutTag) const;
	bool ResolveStatUpOperationSetByCallerTag(FGameplayTag& OutTag) const;

	void NotifyAbilitiesChanged();
	void NotifyPandoraSourceReplicated(UPandoraSkillRuntimeContext* Source);

	FPdAbilitiesChangedNativeDelegate OnAbilitiesChangedNative;

	UPROPERTY(BlueprintAssignable, Category = "!AbilitySystem|Abilities")
	FPdAbilitiesChangedDynamicDelegate OnAbilitiesChanged;

protected:
	virtual void OnRep_ActivateAbilities() override;
	virtual void ClientActivateAbilitySucceedWithEventData_Implementation(
		FGameplayAbilitySpecHandle Handle, FPredictionKey PredictionKey, FGameplayEventData TriggerEventData) override;
	virtual void ClientEndAbility_Implementation(FGameplayAbilitySpecHandle Handle, FGameplayAbilityActivationInfo ActivationInfo) override;
	virtual void ClientCancelAbility_Implementation(FGameplayAbilitySpecHandle Handle, FGameplayAbilityActivationInfo ActivationInfo) override;

private:
	void ReplaySourceReadyActivations();
	TArray<FPendingAbilityInfo> PendingSourceActivations;

	void ReplayReleasedPressInputAfterActivation(FGameplayAbilitySpecHandle AbilityHandle);
	int32 RemoveRuntimeEffects(const FGameplayTagContainer& EffectTags, const FGameplayTagContainer& OwnedTags,
		const FGameplayTagContainer& LooseTags, const FGameplayTagContainer& GameplayCues);

	UPROPERTY(VisibleAnywhere, Instanced, Category = "!AbilitySystem|Runtime")
	TObjectPtr<UAbilityAttributeRuntime> AttributeRuntime;

	UPROPERTY(VisibleAnywhere, Instanced, Category = "!AbilitySystem|Runtime")
	TObjectPtr<UAbilityCollectionRuntime> CollectionRuntime;

	UPROPERTY(Transient)
	bool bResettingAbilityRuntimeState = false;
};
