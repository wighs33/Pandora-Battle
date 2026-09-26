#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "TimerManager.h"
#include "ControllerProfileSyncComponent.generated.h"

class APdPlayerController;

/**
 * 로컬 외형 프로필을 서버 권한의 플레이어 상태 컴포넌트와 연결한다.
 *
 * 비랭크 리슨 서버 환경이므로 원격 프로필 데이터는 검증되지 않은 외형 선호 정보로 취급한다.
 * 제출된 이름은 서버의 정식 스킨 정의를 기준으로만 조회한다.
 */
UCLASS(ClassGroup = (PlayerController))
class LABPROJECT_API UControllerProfileSyncComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Public API ------------------------------------------------------------------------------------------------------
	UControllerProfileSyncComponent();

	void ScheduleLocalCosmeticProfileSync();
	void ApplyGameVictoryGoldReward(const FString& PlayerId, int32 GoldReward) const;
	void ApplyCollectedItemCount(const FString& PlayerId, int32 ItemCount) const;
	void ApplySubmittedLocalCosmeticProfileOnServer(
		const TArray<FName>& OwnedSkinNames,
		FName SelectedAchievementId);

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	void PushLocalCosmeticProfileToServer();
	void HandleSteamAchievementStateChanged();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	APdPlayerController* GetPdController() const;
	FString ResolveRewardPlayerId(const FString& FallbackPlayerId) const;
	void GrantDefaultSkinEntitlementsOnServer() const;
	void CompleteLocalCosmeticProfileSyncAttempt();
	bool TryConsumeRemoteSkinSyncRequest();
	void BindSteamAchievementStateChanged();
	void UnbindSteamAchievementStateChanged();

private:
	FTimerHandle LocalCosmeticProfileSyncTimerHandle;
	FDelegateHandle SteamAchievementStateChangedHandle;
	int32 LocalCosmeticProfileSyncAttemptCount = 0;
	double LastRemoteSkinSyncRequestTime = -1.0;
};
