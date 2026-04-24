#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "FactionInterface.generated.h"

UINTERFACE(MinimalAPI)
class UFactionInterface : public UInterface
{
	GENERATED_BODY()
};

class LABPROJECT_API IFactionInterface
{
	GENERATED_BODY()

public:
	virtual int32 GetFactionId() const = 0;
};
