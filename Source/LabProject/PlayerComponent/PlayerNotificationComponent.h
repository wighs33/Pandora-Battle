#pragma once

#include "CoreMinimal.h"
#include "Components/PlayerStateComponent.h"
#include "UI/NotificationData.h"
#include "PlayerNotificationComponent.generated.h"

class APdPlayerState;
class UObject;
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
		const TArray<FPrimaryAssetId>& RewardPandoraDefinitions) const;

private:
	void SendRewardNotificationForAsset(const FPrimaryAssetId& AssetId, const FText& FallbackText) const;
	UObject* ResolvePrimaryAssetObject(const FPrimaryAssetId& AssetId) const;
	APdPlayerState* GetPdPlayerState() const;
};
