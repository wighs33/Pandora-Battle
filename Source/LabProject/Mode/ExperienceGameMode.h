#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "UObject/PrimaryAssetId.h"
#include "ExperienceGameMode.generated.h"

class UExperienceDefinition;

DECLARE_LOG_CATEGORY_EXTERN(PdExperienceGameModeLog, Log, All);

UCLASS(Blueprintable)
class LABPROJECT_API AExperienceGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AExperienceGameMode(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//------------------------------------------------------------------------------------------------------------------
	//--- Engine Events
	virtual void InitGameState() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;

protected:
	//------------------------------------------------------------------------------------------------------------------
	//--- Experience Flow
	bool IsExperienceLoaded() const;
	void StartExperienceLoad();
	void HandleExperienceLoaded(const UExperienceDefinition* Experience);
	FPrimaryAssetId GetConfiguredExperienceId() const;

protected:
	//------------------------------------------------------------------------------------------------------------------
	//--- Experience Setup
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Experience", meta = (AllowedTypes = "ExperienceDefinition"))
	FPrimaryAssetId DefaultExperienceId;

private:
	//------------------------------------------------------------------------------------------------------------------
	//--- Runtime State
	bool bWaitingForExperience = false;
};
