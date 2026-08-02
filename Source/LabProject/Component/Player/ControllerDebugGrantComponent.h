#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Definition/Player/PlayerControllerDefinition.h"
#include "ControllerDebugGrantComponent.generated.h"

class APdPlayerController;

/**
 * Non-shipping test-resource grant service for APdPlayerController.
 */
UCLASS(ClassGroup = (PlayerController))
class LABPROJECT_API UControllerDebugGrantComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UControllerDebugGrantComponent();

	void ApplySettings(const FPdControllerDebugGrantSettings& InSettings) { Settings = InSettings; }
	void RequestGrantTestResources();
	void GrantTestResourcesOnServer() const;

private:
	APdPlayerController* GetPdController() const;
	void CollectAllWeaponDefinitionIds(TArray<FPrimaryAssetId>& OutWeaponDefinitionIds) const;

	UPROPERTY(Transient)
	FPdControllerDebugGrantSettings Settings;
};
