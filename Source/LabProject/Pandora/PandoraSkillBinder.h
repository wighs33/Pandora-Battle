#pragma once

#include "Common/Enum_Direction.h"
#include "CoreMinimal.h"
#include "GameplayAbilitySpecHandle.h"

class UPandoraDefinition;
class UPandoraSkillRuntimeContext;
class UPdAbilitySystemComponent;

struct LABPROJECT_API FPandoraSkillBindingResult
{
	TArray<FGameplayAbilitySpecHandle> AbilityHandles;
	TArray<TObjectPtr<UPandoraSkillRuntimeContext>> RuntimeContexts;
};

class LABPROJECT_API FPandoraSkillBinder
{
public:
	static FPandoraSkillBindingResult GrantPandoraContent(
		UObject* ContextOuter,
		AActor* AuthorityOwner,
		UPdAbilitySystemComponent* AbilitySystemComponent,
		const UPandoraDefinition* PandoraDefinition,
		int32 PandoraLevel,
		EEnum_Direction LoadoutDirection = EEnum_Direction::Center);

	static void RemoveGrantedContent(
		UPdAbilitySystemComponent* AbilitySystemComponent,
		const TArray<FGameplayAbilitySpecHandle>& AbilityHandles);

	static void RefreshInputBindings(
		UPdAbilitySystemComponent* AbilitySystemComponent,
		const UPandoraDefinition* PandoraDefinition,
		int32 PandoraLevel,
		bool bShouldBindSelectedPandora);
};
