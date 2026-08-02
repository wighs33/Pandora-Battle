#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SkinInstance.generated.h"

class USkinDefinition;
DECLARE_LOG_CATEGORY_EXTERN(SkinInstanceLog, Log, All);

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API USkinInstance : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "!Skin")
	TObjectPtr<const USkinDefinition> SkinDefinition;
};
