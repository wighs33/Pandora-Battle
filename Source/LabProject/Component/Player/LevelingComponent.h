#pragma once

#include "CoreMinimal.h"
#include "Components/PlayerStateComponent.h"
#include "GameplayTagContainer.h"
#include "LevelingComponent.generated.h"

class UGameplayEffect;
class UPdAbilitySystemComponent;
class APlayerState;
class URewardDefinition;
struct FStreamableHandle;

DECLARE_LOG_CATEGORY_EXTERN(LogLevelingComponent, Log, All);

UCLASS(BlueprintType, Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LABPROJECT_API ULevelingComponent : public UPlayerStateComponent
{
	GENERATED_BODY()

public:
	ULevelingComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Server-only C++ entry point for trusted reward producers. */
	bool GrantRewardExperience(int32 ExperienceAmount);

	/** Resolves and grants the configured player-kill reward on the server. */
	bool GrantKillExperience(APlayerState* VictimPlayerState);

	UFUNCTION(BlueprintPure, Category = "!Leveling")
	float GetRequiredExperienceForNextLevel() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool GrantExperienceInternal(float ExperienceAmount);
	bool ApplyLevelUpInternal();
	bool ProcessAutoLevelUps();
	UPdAbilitySystemComponent* GetPdAbilitySystemComponent() const;
	bool GetCurrentAttributeValue(FGameplayTag StatTag, float& OutValue) const;
	const URewardDefinition* GetPlayerKillRewardDefinition() const;
	void BeginPlayerKillRewardPreload();
	void HandlePlayerKillRewardPreloadComplete();
	void ReleasePlayerKillRewardPreload();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Leveling|Effect", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> LevelingGameplayEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Leveling|Tags", meta = (Categories = "Status", AllowPrivateAccess = "true"))
	FGameplayTag LevelStatTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Leveling|Tags", meta = (Categories = "Status", AllowPrivateAccess = "true"))
	FGameplayTag ExperienceStatTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Leveling|Tags", meta = (Categories = "Status", AllowPrivateAccess = "true"))
	TArray<FGameplayTag> CategoryPointStatTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Leveling|Rules", meta = (ClampMin = "0.0", AllowPrivateAccess = "true"))
	float PointsPerCategoryOnLevelUp = 2.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Leveling|Rules", meta = (ClampMin = "0.0", AllowPrivateAccess = "true"))
	float RequiredExperienceForNextLevel = 100.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Leveling|Rewards", meta = (ClampMin = "0.0", AllowPrivateAccess = "true"))
	float PlayerKillExperienceReward = 25.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Leveling|Rewards", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<URewardDefinition> PlayerKillRewardDefinition;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Leveling|Rules", meta = (AllowPrivateAccess = "true"))
	bool bAutoLevelUpWhenExperienceReached = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Leveling|Rules", meta = (AllowPrivateAccess = "true", DisplayName = "Spend Required Experience On Level Up"))
	bool bResetExperienceOnLevelUp = true;

	UPROPERTY(Transient)
	TObjectPtr<URewardDefinition> LoadedPlayerKillRewardDefinition;

	TSharedPtr<FStreamableHandle> PlayerKillRewardPreloadHandle;
};
