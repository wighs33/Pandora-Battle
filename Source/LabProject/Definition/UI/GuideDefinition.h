#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Settings/UiLanguage.h"
#include "GuideDefinition.generated.h"

class UFont;
class UTexture2D;


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
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Guide|Localization")
	EGuideLanguage DefaultLanguage = EGuideLanguage::Korean;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Guide|Localization")
	TArray<EGuideLanguage> SupportedLanguages = {
		EGuideLanguage::English,
		EGuideLanguage::SimplifiedChinese,
		EGuideLanguage::Russian,
		EGuideLanguage::Spanish,
		EGuideLanguage::PortugueseBrazil,
		EGuideLanguage::German,
		EGuideLanguage::Japanese,
		EGuideLanguage::French,
		EGuideLanguage::Polish,
		EGuideLanguage::Korean,
		EGuideLanguage::TraditionalChinese,
		EGuideLanguage::Turkish
	};

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Guide|Localization|Font", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UFont> PolishContentFont;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Guide|Localization|Font", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UFont> TurkishContentFont;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Guide")
	TArray<FGuidePageEntry> Pages;
};
