#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "MatchRewardComponent.generated.h"

class AController;
class AExperienceGameMode;
class APlayerState;
class ARewardChest;
class FContentLease;
class URewardDefinition;

/**
 * 서버에서 경기 보상을 지급한다.
 *
 * 경기 시작 때 맵에 놓인 보상 상자 중 이번 경기에 쓸 상자를 고르고, 경기가 끝나면 승리 팀에게 승리 골드를 준다.
 * 보상 수치와 상자 설정은 GameMode Blueprint에 둔다.
 */
UCLASS(ClassGroup = (Match))
class LABPROJECT_API UMatchRewardComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Public API ------------------------------------------------------------------------------------------------------
	UMatchRewardComponent();

	/** 상자 설정을 비동기로 읽고, 월드의 모든 액터가 BeginPlay를 마친 뒤 활성 상자를 고른다. */
	void PreloadRewardContent();

	/** 경기가 끝나면 아직 끝나지 않은 상자 배치를 멈춘다. */
	void StopChestConfiguration();

	/** 승리 팀 전원(팀이 없으면 승자 한 명)에게 승리 골드를 준다. 나간 플레이어는 받지 않는다. */
	void GrantVictoryGold(
		const APlayerState* WinnerPlayerState,
		int32 WinnerTeamColorIndex,
		int32 WinnerTeamMemberCount,
		const APlayerState* ExcludedPlayerState = nullptr) const;

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	void HandleRewardContentLoaded();
	void HandleWorldBeginPlay();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void ConfigureRewardChestSpawns();
	void UnbindChestConfigurationEvents();
	const URewardDefinition* ResolveRewardDefinitionForChestSpawns(const TArray<ARewardChest*>& RewardChests) const;
	void GrantVictoryGoldToController(AController* WinnerController, int32 WinnerTeamMemberCount) const;
	AExperienceGameMode* GetExperienceGameMode() const;

private:
	bool bChestConfigurationStopped = false;
	FDelegateHandle WorldBeginPlayHandle;
	TArray<TPair<TWeakObjectPtr<ARewardChest>, FDelegateHandle>> PendingChestContentHandles;
	TSharedPtr<FContentLease> RewardContentLease;
};
