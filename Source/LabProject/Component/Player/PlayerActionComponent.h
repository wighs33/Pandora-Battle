#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"

#include "PlayerActionComponent.generated.h"

class UPlayerPawnDefinition;

/**
 * Owns short-lived player action policy: local cooldown state and the
 * authoritative hit-reaction cancellation request.
 */
UCLASS(ClassGroup = (Player), meta = (BlueprintSpawnableComponent))
class LABPROJECT_API UPlayerActionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayerActionComponent();

	void ApplyDefinition(const UPlayerPawnDefinition* Definition);

	bool RequestCancelHitReactForMovement(float BlendOutTime);

private:
	UFUNCTION(Server, Reliable)
	void ServerCancelHitReactForMovement(float BlendOutTime);

	bool CancelHitReactForMovementLocally(float BlendOutTime);
	FGameplayTagContainer ResolveHitReactCancelTags() const;

	UPROPERTY(Transient)
	FGameplayTagContainer HitReactCancelTags;
};
