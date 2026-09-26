#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"

#include "PlayerActionComponent.generated.h"

class UPlayerPawnDefinition;

/**
 * 로컬 쿨다운 상태와 서버 권한의 피격 반응 취소 요청 등
 * 플레이어의 단기 행동 정책을 관리한다.
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
