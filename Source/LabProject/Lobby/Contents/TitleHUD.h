#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "TitleHUD.generated.h"

class UTitleWidget;
class UGameResultWidget;

UCLASS()
class LABPROJECT_API ATitleHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	void ShowPendingGameResult();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Title|UI")
	TSubclassOf<UTitleWidget> TitleWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Title|UI")
	TSubclassOf<UGameResultWidget> GameResultWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UTitleWidget> TitleWidget;

	UPROPERTY(Transient)
	TObjectPtr<UGameResultWidget> GameResultWidget;
};
