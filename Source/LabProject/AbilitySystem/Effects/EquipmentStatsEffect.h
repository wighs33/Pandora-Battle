#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "EquipmentStatsEffect.generated.h"

/**
 * 장착한 아이템 하나의 능력치. 장착해 있는 동안 유지되는 Infinite 효과이며, 능력치마다 SetByCaller(능력치 태그) 수정자를 둔다.
 * 해제할 때는 적용 핸들을 지우기만 하므로 더했던 값을 거꾸로 빼는 계산이 없다.
 */
UCLASS()
class LABPROJECT_API UEquipmentStatsEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	UEquipmentStatsEffect();

	/** 장비가 속성에 더할 수 있는 능력치 태그. 그 밖의 태그는 효과에 넣지 않는다. */
	static TConstArrayView<FGameplayTag> GetStatTags();

	/** 능력치 태그마다 SetByCaller 값을 채운 적용용 Spec. 아이템에 없는 능력치는 0이다. */
	static FGameplayEffectSpecHandle MakeSpec(
		const UAbilitySystemComponent& AbilitySystem,
		const TMap<FGameplayTag, float>& StatMagnitudes,
		const UObject* SourceObject);
};
