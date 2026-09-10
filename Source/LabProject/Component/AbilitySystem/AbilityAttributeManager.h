#pragma once

#include "Common/Enum_Operation.h"
#include "CoreMinimal.h"
#include "Definition/AbilitySystem/AbilityAttributeConfig.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPath.h"
#include "AbilityAttributeManager.generated.h"

class UGameplayEffect;
class UStatUpgradeDefinition;
class UPdAbilitySystemComponent;

/**
 * ASC별 속성 설정과 능력치 효과를 관리한다.
 *
 * 설정의 우선순위와 기본값 적용 기록은 소유 ASC마다 독립적으로 유지한다.
 */
UCLASS()
class LABPROJECT_API UAbilityAttributeManager : public UObject
{
	GENERATED_BODY()

public:
	// 속성 태그 연결과 등록 우선순위
	int32 AddAttributeConfig(const FAttributeConfig& AttributeConfig);
	void RemoveAttributeConfig(int32 AttributeConfigHandle);
	bool ResolveAttributeFromTag(const FGameplayTag& StatTag, FGameplayAttribute& OutAttribute) const;

	// 서버 초기화와 능력치 변경
	bool ApplyConfiguredAttributeDefaults(UPdAbilitySystemComponent& AbilitySystemComponent, const UStatUpgradeDefinition& Definition);

	bool ApplyAttributeDefaultValue(
		UPdAbilitySystemComponent& AbilitySystemComponent, const FGameplayAttribute& Attribute, float DefaultValue) const;

	bool ApplyStatUpEffectByTags(UPdAbilitySystemComponent& AbilitySystemComponent, TSubclassOf<UGameplayEffect> GameplayEffectClass,
		const TMap<FGameplayTag, float>& StatMagnitudes, EEnum_Operation Operation, float Level) const;

	// GameplayEffect에 값을 전달할 프로젝트 공통 태그
	bool ResolveDamageMagnitudeSetByCallerTag(const UPdAbilitySystemComponent& AbilitySystemComponent, FGameplayTag& OutTag) const;

	bool ResolveStatUpOperationSetByCallerTag(const UPdAbilitySystemComponent& AbilitySystemComponent, FGameplayTag& OutTag) const;

private:
	// 초기화 재진입 방지와 실패 기록 정리
	void ClearConfiguredAttributeDefaultsApplied(const UAttributeSet* AttributeSet, const FSoftObjectPath& DefinitionPath);

	bool HasAppliedConfiguredAttributeDefaults(const UAttributeSet* AttributeSet, const FSoftObjectPath& DefinitionPath) const;

	struct FRegisteredAttributeConfig
	{
		int32 Handle;
		FAttributeConfig Config;
	};

	// 뒤에 등록한 설정을 먼저 조회한다. 제거 시 상대적인 등록 순서는 유지한다.
	TArray<FRegisteredAttributeConfig> ActiveAttributeConfigs;
	int32 NextAttributeConfigHandle = 1;

	TWeakObjectPtr<UAttributeSet> ConfiguredDefaultsAttributeSet;
	FSoftObjectPath ConfiguredDefaultsDefinitionPath;
};
