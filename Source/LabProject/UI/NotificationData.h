#pragma once

#include "CoreMinimal.h"
#include "NotificationData.generated.h"

class UObject;

USTRUCT(BlueprintType)
struct LABPROJECT_API FPdNotificationData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Notification")
	FText Text;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Notification", meta = (AllowedClasses = "/Script/Engine.Texture2D,/Script/Engine.MaterialInterface"))
	TObjectPtr<UObject> IconResource = nullptr;
};
