#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "EquipmentStatGameplayEffect.generated.h"

/**
 * <장비 스탯 적용 GameplayEffect>
 * - SetByCaller로 전달된 스탯 태그/값을 UStatUpExecution에서 동적으로 Attribute로 해석합니다.
 * - 장착 시 양수, 해제 시 음수 값을 넣어 같은 실행 경로로 적용/되돌림을 처리합니다.
 */
UCLASS()
class LABPROJECT_API UEquipmentStatGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UEquipmentStatGameplayEffect(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
