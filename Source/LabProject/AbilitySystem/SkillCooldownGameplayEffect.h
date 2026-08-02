#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "SkillCooldownGameplayEffect.generated.h"

UCLASS()
class LABPROJECT_API USkillCooldownGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	USkillCooldownGameplayEffect(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
