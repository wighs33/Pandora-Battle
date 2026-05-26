#pragma once

#include "ActiveGameplayEffectHandle.h"
#include "CoreMinimal.h"
#include "GameplayAbilitySpecHandle.h"
#include "Pandora/PandoraSkillBinder.h"
#include "PandoraSelectedContent.generated.h"

class UPandoraSkillRuntimeContext;

USTRUCT()
struct LABPROJECT_API FPandoraSelectedContent
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TArray<FGameplayAbilitySpecHandle> AbilityHandles;

	UPROPERTY(Transient)
	TArray<FActiveGameplayEffectHandle> EffectHandles;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPandoraSkillRuntimeContext>> RuntimeContexts;

	void Reset();
	void Capture(FPandoraSkillBindingResult&& BindingResult);
	int32 GetGrantedAbilityCount() const { return AbilityHandles.Num(); }
};
