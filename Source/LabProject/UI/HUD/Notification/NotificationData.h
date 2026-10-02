#pragma once

#include "CoreMinimal.h"
#include "NotificationData.generated.h"

class UObject;

// 오른쪽 알림 목록에 표시할 값. 서버가 보내는 보상 정보는 Common/RewardNotificationTypes.h에 있다.
USTRUCT(BlueprintType)
struct LABPROJECT_API FPdNotificationData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Notification")
	FText Text;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Notification", meta = (AllowedClasses = "/Script/Engine.Texture2D,/Script/Engine.MaterialInterface"))
	TObjectPtr<UObject> IconResource = nullptr;
};
