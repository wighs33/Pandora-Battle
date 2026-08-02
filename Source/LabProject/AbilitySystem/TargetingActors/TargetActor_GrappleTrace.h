#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetActor.h"
#include "TargetActor_GrappleTrace.generated.h"

/**
 * Produces one grapple sphere-trace hit from the locally controlled player's
 * weapon-aim viewpoint. The resulting target data is sent through GAS and is
 * retraced by the server before movement begins.
 */
UCLASS(notplaceable)
class LABPROJECT_API ATargetActor_GrappleTrace : public AGameplayAbilityTargetActor
{
	GENERATED_BODY()

public:
	ATargetActor_GrappleTrace(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void StartTargeting(UGameplayAbility* Ability) override;
	virtual void ConfirmTargetingAndContinue() override;
};
