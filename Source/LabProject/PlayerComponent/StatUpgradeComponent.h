#pragma once

#include "CoreMinimal.h"
#include "Common/Enum_Operation.h"
#include "Components/PlayerStateComponent.h"
#include "GameplayTagContainer.h"
#include "StatUpgradeComponent.generated.h"

class UGameplayEffect;
class UStatUpgradeDefinition;

DECLARE_LOG_CATEGORY_EXTERN(StatUpgradeComponentLog, Log, All);

UCLASS(BlueprintType, Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LABPROJECT_API UStatUpgradeComponent : public UPlayerStateComponent
{
	GENERATED_BODY()

public:
	UStatUpgradeComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "!AbilitySystem|Stat", meta = (GameplayTagFilter = "Status"))
	bool RequestStatUp(FGameplayTag StatTag);

private:
	UFUNCTION(Server, Reliable)
	void ServerRequestStatUp(FGameplayTag StatTag);

	bool ApplyStatUpInternal(FGameplayTag StatTag);
	UStatUpgradeDefinition* LoadStatUpgradeDefinition();
	bool ResolveStatUpButtonSettings(const FGameplayTag& StatTag, float& OutMagnitude, EEnum_Operation& OutOperation) const;
	bool ResolvePairedCurrentResourceStatTag(const FGameplayTag& StatTag, FGameplayTag& OutPairedStatTag) const;
	bool ApplyStatUpEffectByTag(TSubclassOf<UGameplayEffect> GameplayEffectClass, FGameplayTag StatTag, float Magnitude, EEnum_Operation Operation, float Level = 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "!AbilitySystem|Stat", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UStatUpgradeDefinition> StatUpgradeDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UStatUpgradeDefinition> LoadedStatUpgradeDefinition;
};
