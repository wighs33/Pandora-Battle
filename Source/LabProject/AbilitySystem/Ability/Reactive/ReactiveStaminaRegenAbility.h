#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "Delegates/IDelegateInstance.h"
#include "GameplayEffectTypes.h"
#include "ReactiveStaminaRegenAbility.generated.h"

class UGameplayEffect;
struct FOnAttributeChangeData;

UCLASS(Blueprintable)
class LABPROJECT_API UReactiveStaminaRegenAbility : public UPdGameplayAbility
{
	GENERATED_BODY()

public:
	UReactiveStaminaRegenAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void OnAbilityEnding() override;

	UFUNCTION()
	void ApplyStaminaRegenEffect();

	void HandleStaminaChanged(const FOnAttributeChangeData& Data);
	void RemoveStaminaRegenEffects();
	void ClearRegenDelayTimer();
	float GetCurrentMaxStamina() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Stamina")
	TSubclassOf<UGameplayEffect> StaminaRegenEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Stamina", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float StaminaRegenDelay = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Stamina")
	bool bRemoveRegenEffectAtMaxStamina = true;

	FDelegateHandle StaminaChangedDelegateHandle;

	FTimerHandle StaminaRegenDelayTimerHandle;
};
