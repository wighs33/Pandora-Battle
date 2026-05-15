#pragma once

#include "CoreMinimal.h"
#include "GameFeature/Extension/Condition/ExtensionCondition.h"
#include "Templates/SubclassOf.h"
#include "ExtensionCondition_HasInputComponent.generated.h"

class UInputComponent;

USTRUCT(BlueprintType, meta = (DisplayName = "Has Input Component"))
struct LABPROJECT_API FExtensionCondition_HasInputComponent : public FExtensionCondition
{
	GENERATED_BODY()

	virtual bool IsSatisfied(AActor* Owner) const override;

	UPROPERTY(EditAnywhere, Category = "Condition", meta = (AllowAbstract = "false"))
	TSubclassOf<UInputComponent> RequiredClass;
};
