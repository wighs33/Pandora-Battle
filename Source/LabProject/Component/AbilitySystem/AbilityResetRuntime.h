#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AbilityResetRuntime.generated.h"

class UAbilityCollectionRuntime;
class UPdAbilitySystemComponent;

/**
 * Applies the project's death, respawn, and Pandora reset cleanup rules.
 */
UCLASS()
class LABPROJECT_API UAbilityResetRuntime : public UObject
{
	GENERATED_BODY()

public:
	void ResetAbilityRuntimeState(
		UPdAbilitySystemComponent& AbilitySystemComponent,
		const UAbilityCollectionRuntime& CollectionRuntime,
		bool bPandoraAbilitiesOnly);

	int32 ClearStatusEffectsForRespawn(UPdAbilitySystemComponent& AbilitySystemComponent) const;
	bool IsResettingAbilityRuntimeState() const { return bResettingAbilityRuntimeState; }

private:
	UPROPERTY(Transient)
	bool bResettingAbilityRuntimeState = false;
};
