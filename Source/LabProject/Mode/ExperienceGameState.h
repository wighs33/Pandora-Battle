#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "ExperienceGameState.generated.h"

class UExperienceManagerComponent;

UCLASS()
class LABPROJECT_API AExperienceGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AExperienceGameState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//------------------------------------------------------------------------------------------------------------------
	//--- Experience API
	UExperienceManagerComponent* GetExperienceManagerComponent() const { return ExperienceManagerComponent; }

private:
	//------------------------------------------------------------------------------------------------------------------
	//--- Components
	UPROPERTY(VisibleAnywhere, Category = "!Experience", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UExperienceManagerComponent> ExperienceManagerComponent;
};
