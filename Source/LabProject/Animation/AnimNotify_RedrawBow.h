#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotify_WeaponEvent.h"
#include "AnimNotify_RedrawBow.generated.h"

/**
 * 현재 장착 중인 활의 리드로우 타이밍을 처리하는 애님 노티파이.
 * 활 프리뷰 액터를 다시 생성하고 활 무기 몽타주를 재개한다.
 */
UCLASS()
class LABPROJECT_API UAnimNotify_RedrawBow : public UAnimNotify_WeaponEvent
{
	GENERATED_BODY()

public:
	UAnimNotify_RedrawBow();

	virtual FString GetNotifyName_Implementation() const override;
};
