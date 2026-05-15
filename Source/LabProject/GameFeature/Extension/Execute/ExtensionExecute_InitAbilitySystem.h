#pragma once

#include "CoreMinimal.h"
#include "GameFeature/Extension/Execute/ExtensionExecute.h"
#include "ExtensionExecute_InitAbilitySystem.generated.h"

USTRUCT(BlueprintType, meta = (DisplayName = "Init Ability System"))
struct LABPROJECT_API FExtensionExecute_InitAbilitySystem : public FExtensionExecute
{
	GENERATED_BODY()

	virtual void OnActivate(AActor* Owner) const override;
	virtual void OnDeactivate(AActor* Owner) const override;
};
