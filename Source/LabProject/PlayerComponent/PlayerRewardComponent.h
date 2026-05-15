#pragma once

#include "CoreMinimal.h"
#include "Components/PlayerStateComponent.h"
#include "PlayerRewardComponent.generated.h"

class AActor;
class APdPlayerState;

/**
 * <플레이어 보상 컴포넌트>
 * - 상호작용 보상을 지급합니다.
 * - 아이템, 스킨, 판도라를 처리합니다.
 */
UCLASS(BlueprintType, Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LABPROJECT_API UPlayerRewardComponent : public UPlayerStateComponent
{
	GENERATED_BODY()

public:
	UPlayerRewardComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//------------------------------------------------------------------------------------------------------------------
	//--- Reward Requests
	// 상호작용 보상 적용 요청
	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "!Reward")
	void ApplyInteractRewards(AActor* InteractableActor);

private:
	//------------------------------------------------------------------------------------------------------------------
	//--- Reward
	bool ApplyInteractRewardsInternal(AActor* InteractableActor);

	//------------------------------------------------------------------------------------------------------------------
	//--- Player Services
	APdPlayerState* GetPdPlayerState() const;
};
