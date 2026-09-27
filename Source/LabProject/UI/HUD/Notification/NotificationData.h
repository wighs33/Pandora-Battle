#pragma once

#include "CoreMinimal.h"
#include "UObject/PrimaryAssetId.h"
#include "UObject/SoftObjectPath.h"
#include "NotificationData.generated.h"

class UObject;

UENUM()
enum class EPdRewardNotificationType : uint8
{
	Item,
	Skin,
	Pandora,
	Experience,
	SoulDust
};

// 서버가 확정한 보상 정보만 전달하며, 표시 문구와 아이콘 객체는 클라이언트에서 준비한다.
USTRUCT()
struct LABPROJECT_API FPdRewardNotification
{
	GENERATED_BODY()

	UPROPERTY()
	EPdRewardNotificationType Type = EPdRewardNotificationType::Item;

	UPROPERTY()
	FPrimaryAssetId AssetId;

	UPROPERTY()
	double Amount = 0.0;

	UPROPERTY()
	FSoftObjectPath IconPath;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FPdNotificationData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Notification")
	FText Text;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Notification", meta = (AllowedClasses = "/Script/Engine.Texture2D,/Script/Engine.MaterialInterface"))
	TObjectPtr<UObject> IconResource = nullptr;
};
