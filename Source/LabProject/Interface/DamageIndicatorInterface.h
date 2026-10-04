#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DamageIndicatorInterface.generated.h"

UINTERFACE(meta = (CannotImplementInterfaceInBlueprint))
class LABPROJECT_API UDamageIndicatorInterface : public UInterface
{
	GENERATED_BODY()
};

/** 피해 숫자를 띄우는 쪽의 계약. 캐릭터는 표시 방식을 모르고 표시 위치를 묻거나 표시를 요청한다. */
class LABPROJECT_API IDamageIndicatorInterface
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	virtual void ShowDamageIndicator(float DamageAmount, FVector WorldLocation, bool bCriticalHit) = 0;
	virtual FVector ResolveDamageIndicatorWorldLocation() const = 0;
};
