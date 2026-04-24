#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "CharacterAbilitySystemInterface.generated.h"

class UPdAbilitySystemComponent;

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UCharacterAbilitySystemInterface : public UAbilitySystemInterface
{
	GENERATED_BODY()
};

class LABPROJECT_API ICharacterAbilitySystemInterface : public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	virtual UPdAbilitySystemComponent* GetPdAbilitySystemComponent() const = 0;
};
