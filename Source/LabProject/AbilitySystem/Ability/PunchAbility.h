#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Ability/AttackAbility.h"
#include "PunchAbility.generated.h"

UCLASS(Blueprintable)
class LABPROJECT_API UPunchAbility : public UAttackAbility
{
	GENERATED_BODY()

public:
	UPunchAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
