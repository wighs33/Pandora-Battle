#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PdAbilityResetRuntime.generated.h"

class UPdAbilityCollectionRuntime;
class UPdAbilitySystemComponent;
struct FPdGameplayEffectRemovalPolicy;

/**
 * Applies data-driven death, respawn, and Pandora reset policies.
 */
UCLASS()
class LABPROJECT_API UPdAbilityResetRuntime : public UObject
{
	GENERATED_BODY()

public:
	void ResetAbilityRuntimeState(
		UPdAbilitySystemComponent& AbilitySystemComponent,
		const UPdAbilityCollectionRuntime& CollectionRuntime,
		bool bPandoraAbilitiesOnly);

	int32 ClearStatusEffectsForRespawn(UPdAbilitySystemComponent& AbilitySystemComponent) const;
	bool IsResettingAbilityRuntimeState() const { return bResettingAbilityRuntimeState; }

private:
	int32 RemoveActiveEffectsMatchingPolicy(
		UPdAbilitySystemComponent& AbilitySystemComponent,
		const FPdGameplayEffectRemovalPolicy& RemovalPolicy) const;

	UPROPERTY(Transient)
	bool bResettingAbilityRuntimeState = false;
};
