#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "TargetingInterface.generated.h"

UINTERFACE(BlueprintType)
class LABPROJECT_API UTargetingInterface : public UInterface
{
	GENERATED_BODY()
};

class LABPROJECT_API ITargetingInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "!Targeting")
	AActor* GetAttackTarget() const;
};
