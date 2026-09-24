#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Settings/UiLanguage.h"
#include "MenuTextRow.generated.h"

/** One stable UI key per row; one editable spreadsheet column per supported language. */
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
