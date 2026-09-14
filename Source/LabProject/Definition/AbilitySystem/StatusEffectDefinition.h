#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "StatusEffectDefinition.generated.h"

class UGameplayEffect;
class UTexture2D;
class UAbilitySystemComponent;
struct FGameplayEffectSpecHandle;

namespace StatusEffectTiming
{
	// 스택 누적이 갱신된 뒤 감소를 시작하기까지 기다리는 시간(초).
	inline constexpr float StackHoldSeconds = 1.0f;
	// 대기 시간을 포함하여 최대 스택 분량이 모두 감소하는 데 걸리는 전체 시간(초).
	inline constexpr float FullStackLifetimeSeconds = 20.0f;
}

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API UStatusEffectDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	bool CanStack(const UAbilitySystemComponent* TargetAbilitySystemComponent) const;
	void RemoveStacks(UAbilitySystemComponent* TargetAbilitySystemComponent) const;

	float GetDamageMagnitude() const { return DamageMagnitude; }

	void SynchronizeStackEffectStackLimit() const;

	/** 발동 전 누적 효과를 식별하는 태그. 스택 조회와 누적 효과 제거에 사용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!StatusEffect|Debuff", meta = (Categories = "Debuff"))
	FGameplayTag StackTag;

	/** 적중 시 대상에게 적용하여 상태 이상 스택을 쌓는 GameplayEffect 클래스. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!StatusEffect|Debuff", meta = (DisplayName = "Debuff Gameplay Effect Class"))
	TSubclassOf<UGameplayEffect> StackGameplayEffectClass;

	/** 누적 가능한 최대 스택 수이자 상태 이상 발동에 필요한 스택 수. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!StatusEffect|Debuff", meta = (ClampMin = "1"))
	int32 MaxStackCount = 1;

	/** 발동한 상태 이상을 나타내는 태그. 효과에 부여하며, 유지 중에는 같은 상태의 추가 누적을 막는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!StatusEffect", meta = (Categories = "Status"))
	FGameplayTag StatusEffectTag;

	/** 누적 스택이 발동 기준에 도달했을 때 대상에게 적용하는 실제 상태 이상 GameplayEffect 클래스. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!StatusEffect")
	TSubclassOf<UGameplayEffect> StatusEffectClass;

	/** 발동한 상태 이상의 지속시간(초). 0이면 GameplayEffect 자체의 지속시간 설정을 사용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!StatusEffect",
		meta = (ClampMin = "0.0", ForceUnits = "s"))
	float StatusDuration = 10.0f;

	/** 상태 이상 피해 계산에 사용하는 기본 피해량. 스킬 피해 계산과 상태별 피해 보너스 적용 전의 값이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!StatusEffect|Damage", meta = (ClampMin = "0.0"))
	float DamageMagnitude = 5.0f;

	/** 상태 이상 UI에 표시할 아이콘 텍스처. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!StatusEffect|UI", meta = (AssetBundles = "Client"))
	TObjectPtr<UTexture2D> Icon;

	/** 상태 이상 UI의 누적 스택 게이지와 남은 지속시간 게이지에 적용할 채움 색상. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!StatusEffect|UI")
	FLinearColor IconBackgroundColor = FLinearColor::White;
};
