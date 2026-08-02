#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GuideDefinition.generated.h"

class UTexture2D;

UENUM(BlueprintType)
enum class EGuideLanguage : uint8
{
	Korean UMETA(DisplayName = "Korean (ko)"),
	English UMETA(DisplayName = "English (en)"),
	Japanese UMETA(DisplayName = "Japanese (ja)"),
	SimplifiedChinese UMETA(DisplayName = "Simplified Chinese (zh-Hans)"),
	Spanish UMETA(DisplayName = "Spanish (es)")
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FGuidePageEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Guide")
	FName ButtonWidgetName = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Guide", meta = (MultiLine = "true"))
	FText Content;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Guide|Localization", meta = (MultiLine = "true"))
	TMap<EGuideLanguage, FText> LocalizedContent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Guide", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UTexture2D> Image;
};

UCLASS(BlueprintType)
class LABPROJECT_API UGuideDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Guide|Localization")
	EGuideLanguage DefaultLanguage = EGuideLanguage::Korean;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Guide")
	TArray<FGuidePageEntry> Pages;
};
