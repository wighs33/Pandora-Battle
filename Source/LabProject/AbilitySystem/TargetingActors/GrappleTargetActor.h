#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetActor.h"
#include "GrappleTargetActor.generated.h"

/**
 * 로컬 플레이어의 무기 조준 시점에서 구형 트레이스로 그래플 적중 결과 하나를 생성한다.
 * 결과는 GAS 대상 데이터로 전송하며, 서버에서 다시 트레이스한 뒤 이동을 시작한다.
 */
UCLASS(notplaceable)
class LABPROJECT_API AGrappleTargetActor : public AGameplayAbilityTargetActor
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void StartTargeting(UGameplayAbility* Ability) override;
	virtual void ConfirmTargetingAndContinue() override;

	// Public API ------------------------------------------------------------------------------------------------------
	AGrappleTargetActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
