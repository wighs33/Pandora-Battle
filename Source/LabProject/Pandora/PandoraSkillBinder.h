#pragma once

#include "CoreMinimal.h"
#include "Common/Enum_Direction.h"
#include "GameplayAbilitySpecHandle.h"

class UPandoraDefinition;
class UPdAbilitySystemComponent;

class LABPROJECT_API FPandoraSkillBinder
{
public:
	// 이미 부여된 스킬은 재사용하고, 새로 열린 스킬의 핸들만 반환한다.
	static TArray<FGameplayAbilitySpecHandle> GrantPandoraContent(
		UPdAbilitySystemComponent* AbilitySystemComponent, const UPandoraDefinition* PandoraDefinition,
		int32 PandoraLevel, EEnum_Direction LoadoutDirection = EEnum_Direction::Center);

	static void RemoveGrantedContent(UPdAbilitySystemComponent* AbilitySystemComponent,
		const TArray<FGameplayAbilitySpecHandle>& AbilityHandles);

	static void RefreshInputBindings(UPdAbilitySystemComponent* AbilitySystemComponent,
		const UPandoraDefinition* PandoraDefinition, int32 PandoraLevel, bool bShouldBindSelectedPandora);
};
