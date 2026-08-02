#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RoomGameMode.generated.h"

UCLASS()
class LABPROJECT_API ARoomGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARoomGameMode(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
