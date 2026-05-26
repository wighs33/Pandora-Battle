#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayTagContainer.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/Skills/SkillTypes.h"
#include "PdGameplayAbility.generated.h"

class APdCharacterBase;
class APdPlayerController;
class APdPlayerState;
class UGameplayEffect;
class USkillDataAsset;
class UPandoraSkillRuntimeContext;
class UPdAbilitySystemComponent;

DECLARE_LOG_CATEGORY_EXTERN(PdGameplayAbilityLog, Log, All);

/**
 * <프로젝트 공용 GameplayAbility 베이스>
 * - 프로젝트에서 공통으로 사용하는 Ability 유틸리티를 제공합니다.
 * - Character, Controller, PlayerState, ASC 접근 함수를 제공합니다.
 * - Ability 부여, Effect 적용, 타깃 탐색 기능을 공통 처리합니다.
 */
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

	UFUNCTION(BlueprintPure, Category = "!Ability|Activation")
	bool ShouldAutoActivateWhenGranted() const { return bAutoActivateWhenGranted; }

	UFUNCTION(BlueprintPure, Category = "!Ability|Skill")
	USkillDataAsset* GetSourceSkillDataAsset() const;

	UFUNCTION(BlueprintPure, Category = "!Ability|Skill")
	UPandoraSkillRuntimeContext* GetSourceSkillRuntimeContext() const;

	TArray<FProjectileImpactEffectAreaSpawnConfig> GetSourceProjectileImpactEffectAreasForLevel(int32 Level) const;

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
	int32 RemoveGameplayEffects(const TArray<TSubclassOf<UGameplayEffect>>& GameplayEffectClasses);

	UFUNCTION(BlueprintCallable, Category = "!Ability|Effect")
	bool RemoveGameplayEffect(TSubclassOf<UGameplayEffect> GameplayEffectClass);

	UFUNCTION(BlueprintCallable, Category = "!Ability|Effect")
	int32 RemoveGameplayEffectsWithGrantedTags(const FGameplayTagContainer& GrantedTags);
	
	UFUNCTION(BlueprintCallable, Category = "!Ability|Targeting")
	bool GetClosestEnemy(AActor*& ClosestEnemy, bool& bLeftOrRight, float SearchRadius = 350.f, float ForwardOffset = 50.f) const;

	UFUNCTION(BlueprintPure, Category = "!Ability|Targeting")
	bool HasPlayerController() const;

	UFUNCTION(BlueprintPure, Category = "!Ability|Targeting")
	AActor* GetAttackTargetFromAvatar() const;
	
protected:
	FActiveGameplayEffectHandle ApplyGameplayEffectHandle(TSubclassOf<UGameplayEffect> GameplayEffectClass, float EffectLevel = 1.f, int32 StackCount = 1);
	FActiveGameplayEffectHandle ApplyGameplayEffectHandle(TSubclassOf<UGameplayEffect> GameplayEffectClass, const FGameplayTagContainer& DynamicGrantedTags,
		float EffectLevel = 1.f, int32 StackCount = 1);
	bool HasActiveGameplayEffect(TSubclassOf<UGameplayEffect> GameplayEffectClass) const;
	bool GrantAbilityIfMissing(TSubclassOf<UGameplayAbility> AbilityClass, int32 AbilityLevel);
	bool HasGrantedAbility(TSubclassOf<UGameplayAbility> AbilityClass) const;
	const FGameplayAbilitySpec* ResolveCurrentAbilitySpec() const;
	UObject* GetCurrentAbilitySpecSourceObject() const;
	UPandoraSkillRuntimeContext* ResolveSourceSkillRuntimeContextFromSelectedPandora() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability|Activation")
	bool bAutoActivateWhenGranted = false;

	UPROPERTY(Transient)
	TObjectPtr<UPandoraSkillRuntimeContext> CachedResolvedSourceSkillRuntimeContext;
};
