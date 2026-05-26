#pragma once

#include "ActiveGameplayEffectHandle.h"
#include "CoreMinimal.h"
#include "GameplayAbilitySpecHandle.h"

class UPandoraDefinition;
class UPandoraSkillRuntimeContext;
class UPdAbilitySystemComponent;

struct LABPROJECT_API FPandoraSkillBindingResult
{
	TArray<FGameplayAbilitySpecHandle> AbilityHandles;
	TArray<FActiveGameplayEffectHandle> EffectHandles;
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
		int32 PandoraLevel);

	static void RemoveGrantedContent(
		UPdAbilitySystemComponent* AbilitySystemComponent,
		const TArray<FGameplayAbilitySpecHandle>& AbilityHandles,
		const TArray<FActiveGameplayEffectHandle>& EffectHandles);

	static void RefreshInputBindings(
		UPdAbilitySystemComponent* AbilitySystemComponent,
		const UPandoraDefinition* PandoraDefinition,
		int32 PandoraLevel,
		bool bShouldBindSelectedPandora);

	static int32 MaxPandoraSlots();
};
