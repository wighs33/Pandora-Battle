#pragma once

#include "CoreMinimal.h"
#include "GameFeature/Extension/Condition/ExtensionCondition.h"
#include "ExtensionCondition_NetworkReady.generated.h"

class UAttributeSet;

USTRUCT(BlueprintType, meta = (DisplayName = "Network Ready"))
struct LABPROJECT_API FExtensionCondition_NetworkReady : public FExtensionCondition
{
	GENERATED_BODY()

	virtual bool IsSatisfied(AActor* Owner) const override;

	UPROPERTY(EditAnywhere, Category = "Condition")
	uint8 bRequirePlayerController : 1 = true;

	UPROPERTY(EditAnywhere, Category = "Condition")
	uint8 bRequirePlayerStateLinked : 1 = true;

	UPROPERTY(EditAnywhere, Category = "Condition")
	uint8 bRequireAbilitySystemReady : 1 = true;

	UPROPERTY(EditAnywhere, Category = "Condition", meta = (EditCondition = "bRequireAbilitySystemReady"))
	TArray<TSubclassOf<UAttributeSet>> RequiredAttributeSets;
};
