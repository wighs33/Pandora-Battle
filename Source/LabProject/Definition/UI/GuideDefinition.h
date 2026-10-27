#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GuideDefinition.generated.h"

class UTexture2D;

USTRUCT(BlueprintType)
struct LABPROJECT_API FGuidePageEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Guide")
	FName ButtonWidgetName = NAME_None;

	/** 본문 원문(영어). 번역은 Game 지역화 타깃이 갖는다. Luna는 본문을 "[ 제목 ]" 줄마다 절로 나눠 참고한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Guide", meta = (MultiLine = "true"))
	FText Content;

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
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Guide")
	TArray<FGuidePageEntry> Pages;
};
