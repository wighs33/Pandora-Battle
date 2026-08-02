#pragma once

#include "CoreMinimal.h"
#include "Components/PlayerStateComponent.h"
#include "PlayerRewardComponent.generated.h"

class AActor;
class APdPlayerState;

UCLASS(BlueprintType, Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LABPROJECT_API UPlayerRewardComponent : public UPlayerStateComponent
{
	GENERATED_BODY()

public:
	UPlayerRewardComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//------------------------------------------------------------------------------------------------------------------
	//--- Reward Requests
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
