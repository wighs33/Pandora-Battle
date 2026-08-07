#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "TimerManager.h"
#include "ControllerProfileSyncComponent.generated.h"

class APdPlayerController;

/**
 * Bridges a local cosmetic profile with authoritative player-state components.
 *
 * Remote profile data is intentionally treated as an unverified cosmetic
 * preference for this unranked listen-server project. Submitted names are
 * resolved only against canonical skin definitions on the server.
 */
UCLASS(ClassGroup = (PlayerController))
class LABPROJECT_API UControllerProfileSyncComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UControllerProfileSyncComponent();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void ScheduleLocalCosmeticProfileSync();
	void ApplyGameVictoryGoldReward(const FString& PlayerId, int32 GoldReward) const;
	void ApplyCollectedItemCount(const FString& PlayerId, int32 ItemCount) const;
	void ApplySubmittedLocalCosmeticProfileOnServer(const TArray<FName>& OwnedSkinNames);

private:
	APdPlayerController* GetPdController() const;
	FString ResolveRewardPlayerId(const FString& FallbackPlayerId) const;
	void PushLocalCosmeticProfileToServer();
	void GrantDefaultSkinEntitlementsOnServer() const;
	void CompleteLocalCosmeticProfileSyncAttempt();
	bool TryConsumeRemoteSkinSyncRequest();

	FTimerHandle LocalCosmeticProfileSyncTimerHandle;
	int32 LocalCosmeticProfileSyncAttemptCount = 0;
	double LastRemoteSkinSyncRequestTime = -1.0;
};
