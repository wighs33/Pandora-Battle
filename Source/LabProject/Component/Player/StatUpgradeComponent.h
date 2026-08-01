#pragma once

#include "ActiveGameplayEffectHandle.h"
#include "CoreMinimal.h"
#include "Common/Enum_Operation.h"
#include "Components/PlayerStateComponent.h"
#include "GameplayTagContainer.h"
#include "Misc/Optional.h"
#include "StatUpgradeComponent.generated.h"

class UGameplayEffect;
class UPdAbilitySystemComponent;
class UStatUpgradeDefinition;
struct FStreamableHandle;
struct FOnAttributeChangeData;

DECLARE_LOG_CATEGORY_EXTERN(StatUpgradeComponentLog, Log, All);

UCLASS(BlueprintType, Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LABPROJECT_API UStatUpgradeComponent : public UPlayerStateComponent
{
	GENERATED_BODY()

public:
	UStatUpgradeComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable, Category = "!AbilitySystem|Stat", meta = (GameplayTagFilter = "Status"))
	bool RequestStatUp(FGameplayTag StatTag);

	UFUNCTION(BlueprintCallable, Category = "!AbilitySystem|Stat", meta = (GameplayTagFilter = "Status"))
	bool RequestStatDown(FGameplayTag StatTag);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "!AbilitySystem|Stat|Points")
	bool GrantPointsToAllCategories(float Amount);

	bool ApplyConfiguredAttributeDefaults();

private:
	UFUNCTION(Server, Reliable)
	void ServerRequestStatUp(FGameplayTag StatTag);

	UFUNCTION(Server, Reliable)
	void ServerRequestStatDown(FGameplayTag StatTag);

	bool ApplyStatUpInternal(FGameplayTag StatTag);
	bool ApplyStatDownInternal(FGameplayTag StatTag);
	UStatUpgradeDefinition* LoadStatUpgradeDefinition();
	void BeginStatUpgradeDefinitionPreload();
	void HandleStatUpgradeDefinitionPreloaded(uint32 RequestGeneration);
	void ReleaseStatUpgradeDefinitionPreload();
	bool ResolveStatUpButtonSettings(const FGameplayTag& StatTag, float& OutMagnitude, EEnum_Operation& OutOperation,
		FGameplayTag& OutCostPointTag, float& OutCost) const;
	bool ResolvePairedCurrentResourceStatTag(const FGameplayTag& StatTag, FGameplayTag& OutPairedStatTag) const;
	bool ResolveStatLevelTag(const FGameplayTag& StatTag, FGameplayTag& OutStatLevelTag) const;
	bool RecalculateConfiguredMaxResources();
	bool RecalculateConfiguredMaxResource(const FGameplayTag& ResourceStatTag, TOptional<float> PreviousInvestmentLevel = TOptional<float>(),
		TOptional<float> PerUpgradePercent = TOptional<float>());
	bool RecalculateCompoundedPercentStats();
	bool RecalculateCompoundedPercentStat(const FGameplayTag& StatTag, TOptional<float> PerUpgradePercent = TOptional<float>());
	bool ApplyStatUpgradeEffects(TSubclassOf<UGameplayEffect> GameplayEffectClass, const TMap<FGameplayTag, float>& StatMagnitudes,
		EEnum_Operation Operation, float Level = 1.f);
	UPdAbilitySystemComponent* GetOwnerPdAbilitySystemComponent() const;
	void BindRecoveryAttributeChanged();
	void UnbindRecoveryAttributeChanged();
	void HandleRecoveryAttributeChanged(const FOnAttributeChangeData& Data);
	void HandleRecoveryMaxHealthAttributeChanged(const FOnAttributeChangeData& Data);
	void HandleRecoveryMaxManaAttributeChanged(const FOnAttributeChangeData& Data);
	void StartRecoveryHealthRegen();
	void StopRecoveryHealthRegen();
	void ApplyRecoveryHealthRegenEffect();

	UPROPERTY(EditDefaultsOnly, Category = "!AbilitySystem|Stat", meta = (AllowPrivateAccess = "true", DisplayName = "DA Stat"))
	TSoftObjectPtr<UStatUpgradeDefinition> StatUpgradeDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UStatUpgradeDefinition> LoadedStatUpgradeDefinition;

	TSharedPtr<FStreamableHandle> StatUpgradeDefinitionLoadHandle;
	uint32 StatUpgradeDefinitionLoadGeneration = 0;
	bool bApplyDefaultsWhenDefinitionReady = false;

	FActiveGameplayEffectHandle RecoveryHealthRegenEffectHandle;
	FDelegateHandle RecoveryAttributeChangedDelegateHandle;
	FDelegateHandle RecoveryMaxHealthAttributeChangedDelegateHandle;
	FDelegateHandle RecoveryMaxManaAttributeChangedDelegateHandle;
};
