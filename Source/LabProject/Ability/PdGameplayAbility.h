#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayTagContainer.h"
#include "Abilities/GameplayAbility.h"
#include "PdGameplayAbility.generated.h"

class APdCharacterBase;
class APdPlayerController;
class APdPlayerState;
class UGameplayEffect;
class UPdAbilitySystemComponent;

DECLARE_LOG_CATEGORY_EXTERN(PdGameplayAbilityLog, Log, All);

UCLASS(Abstract, Blueprintable)
class LABPROJECT_API UPdGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UPdGameplayAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// 게터
public:
	UFUNCTION(BlueprintPure, Category = "!Ability")
	APdCharacterBase* GetPdCharacterFromActorInfo() const;

	UFUNCTION(BlueprintPure, Category = "!Ability")
	APdPlayerController* GetPdPlayerControllerFromActorInfo() const;

	UFUNCTION(BlueprintPure, Category = "!Ability")
	APdPlayerState* GetPdPlayerStateFromActorInfo() const;

	UFUNCTION(BlueprintPure, Category = "!Ability")
	UPdAbilitySystemComponent* GetPdAbilitySystemComponentFromActorInfo() const;

	// Activation
public:
	UFUNCTION(BlueprintCallable, Category = "!Ability|Activation")
	bool TryActivateAbilitiesByTags(FGameplayTagContainer InAbilityTags, bool bAllowRemoteActivation = true) const;

	UFUNCTION(BlueprintCallable, Category = "!Ability|Grant")
	int32 GrantAbilities(const TArray<TSubclassOf<UGameplayAbility>>& AbilityClasses, int32 AbilityLevel = 1);

	UFUNCTION(BlueprintCallable, Category = "!Ability|Effect")
	int32 ApplyGameplayEffects(const TArray<TSubclassOf<UGameplayEffect>>& GameplayEffectClasses, float EffectLevel = 1.f, int32 StackCount = 1);

	UFUNCTION(BlueprintCallable, Category = "!Ability|Effect")
	bool ApplyGameplayEffect(TSubclassOf<UGameplayEffect> GameplayEffectClass, float EffectLevel = 1.f, int32 StackCount = 1);

	UFUNCTION(BlueprintCallable, Category = "!Ability|Effect")
	int32 RemoveGameplayEffects(const TArray<FActiveGameplayEffectHandle>& GameplayEffectHandles);

	UFUNCTION(BlueprintCallable, Category = "!Ability|Effect")
	bool RemoveGameplayEffect(FActiveGameplayEffectHandle GameplayEffectHandle);

	UFUNCTION(BlueprintCallable, Category = "!Ability|Effect")
	int32 RemoveGameplayEffectsWithGrantedTags(const FGameplayTagContainer& GrantedTags);
	
	UFUNCTION(BlueprintCallable, Category = "!Ability|Targeting")
	bool GetClosestEnemy(AActor*& ClosestEnemy, bool& bLeftOrRight, float SearchRadius = 350.f, float ForwardOffset = 50.f) const;
	
protected:
	FActiveGameplayEffectHandle ApplyGameplayEffectHandle(TSubclassOf<UGameplayEffect> GameplayEffectClass, float EffectLevel = 1.f, int32 StackCount = 1);
	FActiveGameplayEffectHandle ApplyGameplayEffectHandle(TSubclassOf<UGameplayEffect> GameplayEffectClass, const FGameplayTagContainer& DynamicGrantedTags,
		float EffectLevel = 1.f, int32 StackCount = 1);
	bool GrantAbilityIfMissing(TSubclassOf<UGameplayAbility> AbilityClass, int32 AbilityLevel);
	bool HasGrantedAbility(TSubclassOf<UGameplayAbility> AbilityClass) const;
};
