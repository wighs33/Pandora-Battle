#pragma once

#include "CoreMinimal.h"
#include "Common/Enum_Direction.h"
#include "GameplayAbilitySpecHandle.h"

class UPandoraDefinition;
class UPandoraComponent;
class UPdAbilitySystemComponent;

class LABPROJECT_API FPandoraSkillBinder
{

public:
	// Public API ------------------------------------------------------------------------------------------------------
	// 이미 부여된 스킬은 재사용하고, 새로 열린 스킬의 핸들만 반환한다.
	static TArray<FGameplayAbilitySpecHandle> GrantPandoraContent(
		UPandoraComponent* SourceOwner, UPdAbilitySystemComponent* AbilitySystemComponent, const UPandoraDefinition* PandoraDefinition,
		int32 PandoraLevel, EEnum_Direction LoadoutDirection = EEnum_Direction::Center);

	static void RemoveGrantedContent(UPdAbilitySystemComponent* AbilitySystemComponent,
		const TArray<FGameplayAbilitySpecHandle>& AbilityHandles);

	// 입력 연결만 갱신한다. UI 알림은 호출자가 판도라 구성 변경을 마친 뒤 보낸다.
	static void RefreshInputBindings(UPdAbilitySystemComponent* AbilitySystemComponent,
		const UPandoraDefinition* PandoraDefinition, int32 PandoraLevel, bool bShouldBindSelectedPandora);
};
