#pragma once

#include "CoreMinimal.h"
#include "Components/PlayerStateComponent.h"
#include "GameplayTagContainer.h"
#include "LevelingComponent.generated.h"

class UGameplayEffect;

/**
 * 플레이어의 경험치 지급과 레벨 상승 규칙을 담당한다.
 *
 * 보상의 종류와 지급량은 보상 생산자가 결정하며, 경험치·레벨·포인트의 저장과 복제는 GAS가 담당한다.
 */
UCLASS(BlueprintType, Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LABPROJECT_API ULevelingComponent : public UPlayerStateComponent
{
	GENERATED_BODY()

public:
	bool GrantRewardExperience(int32 ExperienceAmount);

	UFUNCTION(BlueprintPure, Category = "!Leveling")
	float GetRequiredExperienceForNextLevel() const;

private:
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
};
