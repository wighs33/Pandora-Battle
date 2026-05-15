#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PdPlayerController.generated.h"

class UControllerInputComponent;

DECLARE_LOG_CATEGORY_EXTERN(PdPlayerControllerLog, Log, All);

UCLASS()
class LABPROJECT_API APdPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	APdPlayerController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//------------------------------------------------------------------------------------------------------------------
	//--- Engine Callbacks
	virtual void PreInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;

private:
	//------------------------------------------------------------------------------------------------------------------
	//--- Components
	UControllerInputComponent* GetControllerInputComponent() const;
};
