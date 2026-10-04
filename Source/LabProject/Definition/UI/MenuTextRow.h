#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Common/UiLanguage.h"
#include "MenuTextRow.generated.h"

/** 행마다 바뀌지 않는 UI 키 하나와, 지원 언어마다 편집할 스프레드시트 열 하나를 둔다. */
USTRUCT(BlueprintType)
struct LABPROJECT_API FMenuTextRow : public FTableRowBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Korean;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText English;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Japanese;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText SimplifiedChinese;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Spanish;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Russian;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText PortugueseBrazil;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText German;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText French;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Polish;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText TraditionalChinese;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Turkish;

	// Public API ------------------------------------------------------------------------------------------------------
	const FText& GetTranslation(EGuideLanguage Language) const;
	FText Resolve(EGuideLanguage Language) const;
};
