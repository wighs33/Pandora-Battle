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

public:
	// Public API ------------------------------------------------------------------------------------------------------
	virtual bool IsSatisfied(AActor* Owner) const override;

public:
	UPROPERTY(EditAnywhere, Category = "Condition", meta = (AllowAbstract = "false"))
	TSubclassOf<UInputComponent> RequiredClass;
};
