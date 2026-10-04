#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpecHandle.h"

class UPdAbilitySystemComponent;

// 적이 지금 무기로 쓸 공격 능력을 고르고 실행한다. 무기 상태와 능력 목록만 보고 판단하므로 상태를 갖지 않는다.
namespace PdEnemyAttackSelection
{
	/** 무기 상태에 맞는 공격 능력 핸들을 모은다. 공격 태그가 없는 예전 블루프린트 능력은 클래스로 고른다. */
	LABPROJECT_API void GatherAttackAbilityHandles(
		const UPdAbilitySystemComponent& AbilitySystem,
		bool bUsingRangedWeapon,
		bool bHasEquippedWeapon,
		TArray<FGameplayAbilitySpecHandle>& OutAbilityHandles);

	/** 후보 가운데 이미 실행 중인 공격이 있으면 true. 근접 콤보 공격이면 bRequestCombo일 때 다음 구간을 예약한다. */
	LABPROJECT_API bool TryContinueActiveAttack(
		UPdAbilitySystemComponent& AbilitySystem,
		const TArray<FGameplayAbilitySpecHandle>& AbilityHandles,
		bool bRequestCombo);

	/** 후보 순서를 섞어 처음으로 실행에 성공한 공격을 쓴다. */
	LABPROJECT_API bool TryActivateAnyAttack(
		UPdAbilitySystemComponent& AbilitySystem,
		TArray<FGameplayAbilitySpecHandle>& AbilityHandles);

	/** 근접·원거리·주먹 공격 가운데 하나라도 실행 중인지 */
	LABPROJECT_API bool IsAnyAttackActive(const UPdAbilitySystemComponent& AbilitySystem);
}
