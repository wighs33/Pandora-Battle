#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "RoomHUD.generated.h"

class URoomListWidget;

UCLASS()
class LABPROJECT_API ARoomHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Room|UI")
	TSubclassOf<URoomListWidget> RoomListWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<URoomListWidget> RoomListWidget;
};
