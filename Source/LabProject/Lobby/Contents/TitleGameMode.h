#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TitleGameMode.generated.h"

UCLASS()
class LABPROJECT_API ATitleGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void BeginPlay() override;

	// Public API ------------------------------------------------------------------------------------------------------
	ATitleGameMode(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	void TravelDedicatedServerToLobby();
};
