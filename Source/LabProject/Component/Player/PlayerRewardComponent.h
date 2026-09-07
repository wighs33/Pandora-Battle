#pragma once

#include "CoreMinimal.h"
#include "Components/PlayerStateComponent.h"
#include "PlayerRewardComponent.generated.h"

class AActor;
class APdPlayerState;
class APlayerState;
class URewardDefinition;
struct FStreamableHandle;

/**
 * 플레이어의 상호작용 보상과 처치 보상 지급을 담당한다.
 *
 * 경험치 계산은 LevelingComponent에 맡기고, 실제 지급한 보상의 알림을 컨트롤러에 전달한다.
 */
UCLASS(BlueprintType, Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LABPROJECT_API UPlayerRewardComponent : public UPlayerStateComponent
{
	GENERATED_BODY()

public:
	UPlayerRewardComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//------------------------------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "!Reward")
	void ApplyInteractRewards(AActor* InteractableActor);

	void GrantKillExperience(APlayerState* VictimPlayerState);
	void GrantMonsterDefeatRewards(TSoftObjectPtr<URewardDefinition> RewardDefinition);

protected:
	//------------------------------------------------------------------------------------------------------------------
	//--- Engine Callbacks
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	//------------------------------------------------------------------------------------------------------------------
	bool ApplyInteractRewardsInternal(AActor* InteractableActor);
	void HandlePlayerKillRewardLoaded();
	void GrantPlayerKillReward();
	void HandleMonsterRewardLoaded(FSoftObjectPath DefinitionPath);
	void ApplyMonsterDefeatRewards(const URewardDefinition* RewardDefinition);

	//------------------------------------------------------------------------------------------------------------------
	APdPlayerState* GetPdPlayerState() const;

	UPROPERTY(Transient)
	TSoftObjectPtr<URewardDefinition> PlayerKillRewardDefinition;

	UPROPERTY(Transient)
	TObjectPtr<URewardDefinition> LoadedPlayerKillRewardDefinition;

	TSharedPtr<FStreamableHandle> PlayerKillRewardLoadHandle;
	int32 PendingPlayerKillRewards = 0;
	TMap<FSoftObjectPath, int32> PendingMonsterRewards;
	TMap<FSoftObjectPath, TSharedPtr<FStreamableHandle>> MonsterRewardLoadHandles;
};
