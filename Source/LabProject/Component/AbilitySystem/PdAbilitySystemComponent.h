#pragma once

#include "AbilitySystemComponent.h"
#include "Common/Enum_Operation.h"
#include "CoreMinimal.h"
#include "Definition/AbilitySystem/AbilityAttributeConfig.h"
#include "GameplayAbilitySpec.h"
#include "GameplayTagContainer.h"
#include "PdAbilitySystemComponent.generated.h"

class UGameplayAbility;
class UGameplayEffect;
class UPdAbilityAttributeRuntime;
class UPdAbilityCollectionRuntime;
class UPdAbilityResetRuntime;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPdAbilitiesChangedDynamicDelegate);
DECLARE_MULTICAST_DELEGATE(FPdAbilitiesChangedNativeDelegate);

/**
 * Project AbilitySystemComponent facade.
 *
 * GAS integration and compatibility APIs stay here. Focused runtime objects own
 * attribute, ability collection, and reset behavior so the same implementation
 * is reused by PlayerState and enemy-owned ability systems.
 */
UCLASS(BlueprintType, Blueprintable, ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LABPROJECT_API UPdAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UPdAbilitySystemComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void OnRegister() override;
	virtual void OnGiveAbility(FGameplayAbilitySpec& AbilitySpec) override;
	virtual void OnRemoveAbility(FGameplayAbilitySpec& AbilitySpec) override;
	virtual void NotifyAbilityActivated(
		FGameplayAbilitySpecHandle Handle,
		UGameplayAbility* Ability) override;

	bool HasAbilityActorInfoAllocated() const { return AbilityActorInfo.IsValid(); }

	UPdAbilityAttributeRuntime* GetAttributeRuntime() const { return AttributeRuntime.Get(); }
	UPdAbilityCollectionRuntime* GetCollectionRuntime() const { return CollectionRuntime.Get(); }
	UPdAbilityResetRuntime* GetResetRuntime() const { return ResetRuntime.Get(); }

	int32 AddAttributeConfig(const FPdAttributeConfig& AttributeConfig);
	void RemoveAttributeConfig(int32 AttributeConfigHandle);
	bool ApplyAttributeDefaultValue(const FGameplayAttribute& Attribute, float DefaultValue);

	void AbilityInputTagPressed(const FGameplayTag& InputTag);
	void AbilityInputTagReleased(const FGameplayTag& InputTag);

	const FGameplayAbilitySpec* FindActiveAbilitySpecByTags(const FGameplayTagContainer& AbilityTags) const;
	bool HasActiveAbilityWithTags(const FGameplayTagContainer& AbilityTags) const;
	bool HasActiveAbilityOfClass(TSubclassOf<UGameplayAbility> AbilityClass, bool bIncludeChildClasses = true) const;
	bool HasActiveAbilityOfAnyClass(
		const TArray<TSubclassOf<UGameplayAbility>>& AbilityClasses,
		bool bIncludeChildClasses = true) const;

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
	void ResetPandoraAbilityRuntimeState();
	int32 ClearStatusEffectsForRespawn();
	void ReactivateAutoActivatedAbilities();
	bool IsResettingAbilityRuntimeState() const;

	bool ResolveAttributeFromTag(const FGameplayTag& StatTag, FGameplayAttribute& OutAttribute) const;
	bool ResolveDamageMagnitudeSetByCallerTag(FGameplayTag& OutTag) const;
	bool ResolveStatUpOperationSetByCallerTag(FGameplayTag& OutTag) const;

	void NotifyAbilitiesChanged();

	FPdAbilitiesChangedNativeDelegate OnAbilitiesChangedNative;

	UPROPERTY(BlueprintAssignable, Category = "!AbilitySystem|Abilities")
	FPdAbilitiesChangedDynamicDelegate OnAbilitiesChanged;

protected:
	virtual void OnRep_ActivateAbilities() override;

private:
	void ReplayReleasedPressInputAfterActivation(
		FGameplayAbilitySpecHandle AbilityHandle);

	UPROPERTY(VisibleAnywhere, Instanced, Category = "!AbilitySystem|Runtime")
	TObjectPtr<UPdAbilityAttributeRuntime> AttributeRuntime;

	UPROPERTY(VisibleAnywhere, Instanced, Category = "!AbilitySystem|Runtime")
	TObjectPtr<UPdAbilityCollectionRuntime> CollectionRuntime;

	UPROPERTY(VisibleAnywhere, Instanced, Category = "!AbilitySystem|Runtime")
	TObjectPtr<UPdAbilityResetRuntime> ResetRuntime;
};
