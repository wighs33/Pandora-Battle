#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"

#include "PlayerActionComponent.generated.h"

class UPlayerPawnDefinition;

/**
 * 이동 입력에 따른 피격 반응 취소를 로컬에서 예측하고 서버에 요청한다.
 */
UCLASS(ClassGroup = (Player), meta = (BlueprintSpawnableComponent))
class LABPROJECT_API UPlayerActionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	UPlayerActionComponent();

	void ApplyDefinition(const UPlayerPawnDefinition* Definition);

	bool RequestCancelHitReactForMovement(float BlendOutTime);

private:
	// Network RPCs ----------------------------------------------------------------------------------------------------
	UFUNCTION(Server, Reliable)
	void ServerCancelHitReactForMovement(float BlendOutTime);

	// Internal Helpers ------------------------------------------------------------------------------------------------
	bool CancelHitReactForMovementLocally(float BlendOutTime);
	FGameplayTagContainer ResolveHitReactCancelTags() const;

private:
	UPROPERTY(Transient)
	FGameplayTagContainer HitReactCancelTags;
};
