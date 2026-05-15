#pragma once

#include "CoreMinimal.h"
#include "ExtensionCondition.generated.h"

class AActor;

USTRUCT(BlueprintType, meta = (Hidden))
struct LABPROJECT_API FExtensionCondition
{
	GENERATED_BODY()

	virtual ~FExtensionCondition() = default;

	virtual bool IsSatisfied(AActor* Owner) const { return true; }
};
