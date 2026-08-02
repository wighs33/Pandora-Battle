#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PandoraInstance.generated.h"

class UPandoraDefinition;
DECLARE_LOG_CATEGORY_EXTERN(PandoraInstanceLog, Log, All);

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API UPandoraInstance : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "!Pandora")
	TObjectPtr<const UPandoraDefinition> PandoraDefinition;


	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "!Item")
	bool IsOwned = false;
};
