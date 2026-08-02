#pragma once

#include "CoreMinimal.h"
#include "Components/PlayerStateComponent.h"
#include "UI/NotificationData.h"
#include "PlayerNotificationComponent.generated.h"

class APdPlayerState;
class UObject;
struct FStreamableHandle;
struct FPrimaryAssetId;

UCLASS(BlueprintType, Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LABPROJECT_API UPlayerNotificationComponent : public UPlayerStateComponent
{
	GENERATED_BODY()

public:
	UPlayerNotificationComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	void SendNotification(const FPdNotificationData& NotificationData) const;
	void SendRewardNotifications(
		const TArray<FPrimaryAssetId>& RewardItemDefinitions,
		const TArray<FPrimaryAssetId>& RewardSkinDefinitions,
		const TArray<FPrimaryAssetId>& RewardPandoraDefinitions);
	void SendExperienceRewardNotification(float RewardAmount, UObject* IconResource) const;
	void SendSoulDustRewardNotification(int32 RewardAmount, UObject* IconResource) const;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void SendNumericRewardNotification(
		const FText& RewardName,
		float RewardAmount,
		UObject* IconResource) const;
	void SendRewardNotificationForAsset(const FPrimaryAssetId& AssetId, const FText& FallbackText) const;
	void CompleteRewardNotificationLoad(
		uint64 RequestId,
		TArray<FPrimaryAssetId> RewardItemDefinitions,
		TArray<FPrimaryAssetId> RewardSkinDefinitions,
		TArray<FPrimaryAssetId> RewardPandoraDefinitions);
	void SendLoadedRewardNotifications(
		const TArray<FPrimaryAssetId>& RewardItemDefinitions,
		const TArray<FPrimaryAssetId>& RewardSkinDefinitions,
		const TArray<FPrimaryAssetId>& RewardPandoraDefinitions) const;
	void ReleasePendingRewardNotificationLoads();
	UObject* ResolvePrimaryAssetObject(const FPrimaryAssetId& AssetId) const;
	APdPlayerState* GetPdPlayerState() const;

	uint64 NextRewardNotificationRequestId = 1;
	TSet<uint64> ActiveRewardNotificationRequestIds;
	TMap<uint64, TSharedPtr<FStreamableHandle>> PendingRewardNotificationLoadHandles;
};
