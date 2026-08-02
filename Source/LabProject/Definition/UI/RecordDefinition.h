#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RecordDefinition.generated.h"

class UTexture2D;

USTRUCT(BlueprintType)
struct LABPROJECT_API FRecordTierEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Record")
	FText TierName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Record", meta = (ClampMin = "0", UIMin = "0"))
	int32 RankOrder = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Record", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UTexture2D> TierImage;
};

UCLASS(BlueprintType)
class LABPROJECT_API URecordDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	URecordDefinition(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Record", meta = (ClampMin = "1", UIMin = "1"))
	int32 WinsPerTier = 20;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Record", meta = (TitleProperty = "TierName"))
	TArray<FRecordTierEntry> TierEntries;

	UFUNCTION(BlueprintPure, Category = "!Record")
	FRecordTierEntry ResolveTierForWinCount(int32 WinCount) const;
};
