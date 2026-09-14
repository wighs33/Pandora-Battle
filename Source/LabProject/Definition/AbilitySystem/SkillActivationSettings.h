#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SkillActivationSettings.generated.h"

/** 스킬별 GAS 태그와 입력·비용 정책. 실행 클래스와 무관하게 데이터로 정의한다. */
USTRUCT(BlueprintType)
struct LABPROJECT_API FSkillActivationSettings
{
	GENERATED_BODY()

	/** 스킬을 식별하는 태그. 부여된 AbilitySpec에 저장한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tags")
	FGameplayTagContainer Tags;
	/** 실행 중 소유자에게 부여하는 태그. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tags")
	FGameplayTagContainer OwnedTags;
	/** 소유자에게 하나라도 있으면 시전을 차단하는 태그. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tags")
	FGameplayTagContainer BlockedTags;
	/** 시전하려면 소유자가 모두 보유해야 하는 태그. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tags")
	FGameplayTagContainer RequiredTags;
	/** 시전 시 취소할 다른 능력의 태그. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tags")
	FGameplayTagContainer CancelAbilityTags;
	/** 실행 중 차단할 다른 능력의 태그. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tags")
	FGameplayTagContainer BlockAbilityTags;
	/** 쿨다운 효과를 사망·리셋 시 제거하는 정책 태그. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tags")
	FGameplayTagContainer CooldownRemovalTags;
	/** 스킬 입력을 놓았을 때 조준을 확정한다. 별도 확인 입력을 쓰면 끈다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	bool bConfirmTargetingOnInputRelease = true;
	/** 입력 유지 스킬이 너무 짧게 실행되지 않도록 보장하는 최소 시간(초). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (ClampMin = "0.0"))
	float MinimumHoldSeconds = 0.0f;
	/** 쿨다운이 발생하기 전 연속으로 사용할 수 있는 횟수. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cost", meta = (ClampMin = "1"))
	int32 UsesPerCooldown = 1;
	/** 연속 사용 횟수에 스킬 레벨을 곱한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cost")
	bool bScaleUsesWithLevel = false;
};
