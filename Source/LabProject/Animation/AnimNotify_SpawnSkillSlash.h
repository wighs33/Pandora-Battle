#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotify_WeaponEvent.h"
#include "AnimNotify_SpawnSkillSlash.generated.h"

UCLASS()
class LABPROJECT_API UAnimNotify_SpawnSkillSlash : public UAnimNotify_WeaponEvent
{
	GENERATED_BODY()

public:
	UAnimNotify_SpawnSkillSlash();

	virtual FString GetNotifyName_Implementation() const override;
};
