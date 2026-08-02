#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotify_WeaponEvent.h"
#include "AnimNotify_RedrawBow.generated.h"

UCLASS()
class LABPROJECT_API UAnimNotify_RedrawBow : public UAnimNotify_WeaponEvent
{
	GENERATED_BODY()

public:
	UAnimNotify_RedrawBow();

	virtual FString GetNotifyName_Implementation() const override;
};
