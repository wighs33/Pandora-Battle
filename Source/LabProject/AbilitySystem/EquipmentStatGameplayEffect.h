#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "EquipmentStatGameplayEffect.generated.h"

UCLASS()
class LABPROJECT_API UEquipmentStatGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UEquipmentStatGameplayEffect(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
