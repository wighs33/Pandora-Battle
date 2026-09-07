#pragma once

#include "CoreMinimal.h"
#include "Components/ControllerComponent.h"
#include "UI/NotificationData.h"
#include "PlayerNotificationComponent.generated.h"

struct FStreamableHandle;

/**
 * 지급된 보상을 소유 플레이어에게 알리는 컴포넌트.
 *
 * 서버는 보상 정보를 Controller의 RPC로 전달하고, 소유 클라이언트가
 * 표시용 에셋을 비동기로 읽어 알림 문구와 아이콘을 준비한다.
 */
UCLASS(BlueprintType, Blueprintable, ClassGroup=(PlayerController), meta=(BlueprintSpawnableComponent))
class LABPROJECT_API UPlayerNotificationComponent : public UControllerComponent
{
	GENERATED_BODY()

public:
	UPlayerNotificationComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	void SendRewardNotifications(const TArray<FPrimaryAssetId>& RewardItemDefinitions,
		const TArray<FPrimaryAssetId>& RewardSkinDefinitions, const TArray<FPrimaryAssetId>& RewardPandoraDefinitions) const;
	void SendExperienceRewardNotification(float RewardAmount, UObject* IconResource) const;
	void SendSoulDustRewardNotification(int32 RewardAmount, UObject* IconResource) const;
	void ShowRewardNotifications(const TArray<FPdRewardNotification>& Rewards);

protected:
	//--------------------------------------------------------------------------------------------------------------------------------------------
	//--- Engine Callbacks
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	//--------------------------------------------------------------------------------------------------------------------------------------------
	void SendNumericRewardNotification(EPdRewardNotificationType Type, double RewardAmount, UObject* IconResource) const;
	void CompleteRewardNotificationLoad(uint64 RequestId, const TArray<FPdRewardNotification>& Rewards);
	void ShowLoadedRewardNotifications(const TArray<FPdRewardNotification>& Rewards) const;

	uint64 NextRewardNotificationRequestId = 1;
	TMap<uint64, TSharedPtr<FStreamableHandle>> PendingRewardNotificationLoadHandles;
};
