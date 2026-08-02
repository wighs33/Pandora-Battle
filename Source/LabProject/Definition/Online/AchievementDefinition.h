#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AchievementDefinition.generated.h"

class UTexture2D;

UENUM(BlueprintType)
enum class EPdAchievementTrigger : uint8
{
	Manual,
	MatchPlayed,
	WinCount,
	KillCount,
	DeathCount,
	RewardGold,
	PandoraUnlocked,
	SkinUnlocked,
	ItemCollected
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FPdAchievementEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Achievement")
	bool bEnabled = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Achievement", meta = (DisplayName = "Steam Achievement API Name"))
	FString AchievementId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Achievement")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Achievement", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Achievement")
	EPdAchievementTrigger Trigger = EPdAchievementTrigger::Manual;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Achievement", meta = (ClampMin = "1", UIMin = "1"))
	int32 RequiredValue = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Achievement")
	bool bHidden = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Achievement|Icon", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UTexture2D> LockedIcon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Achievement|Icon", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UTexture2D> UnlockedIcon;
};

UCLASS(BlueprintType, Blueprintable, meta = (DisplayName = "Achievement Definition"))
class LABPROJECT_API UAchievementDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Achievement", meta = (TitleProperty = "AchievementId"))
	TArray<FPdAchievementEntry> Achievements;

};
