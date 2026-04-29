#pragma once

#include "CoreMinimal.h"
#include "Components/PlayerStateComponent.h"
#include "PlayerRewardComponent.generated.h"

class AActor;

/**
 * <플레이어 보상 컴포넌트>
 * - 상호작용 보상을 지급합니다.
 * - 아이템, 스킨, 판도라를 처리합니다.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LABPROJECT_API UPlayerRewardComponent : public UPlayerStateComponent
{
	GENERATED_BODY()

public:
	/** 상호작용 보상을 적용합니다. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "!Reward")
	bool ApplyInteractRewards(AActor* InteractableActor);
};