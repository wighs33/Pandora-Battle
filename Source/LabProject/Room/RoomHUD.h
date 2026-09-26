#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "RoomHUD.generated.h"

class URoomListWidget;

class UUiScreen;

UCLASS()
class LABPROJECT_API ARoomHUD : public AHUD
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	UPROPERTY(Transient)
	TObjectPtr<UUiScreen> Screen;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Room|UI")
	TSubclassOf<URoomListWidget> RoomListWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<URoomListWidget> RoomListWidget;
};
