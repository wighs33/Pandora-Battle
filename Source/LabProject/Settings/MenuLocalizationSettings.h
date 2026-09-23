#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Settings/UiLanguage.h"
#include "MenuLocalizationSettings.generated.h"

class UDataTable;
class UFont;
class UGameSettingsWidget;

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Menu Localization"))
class LABPROJECT_API UMenuLocalizationSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	UPROPERTY(Config, EditAnywhere, Category="Localization")
	EGuideLanguage DefaultLanguage = EGuideLanguage::Korean;

	UPROPERTY(Config, EditAnywhere, Category="Localization", meta=(RequiredAssetDataTags="RowStructure=/Script/LabProject.MenuTextRow"))
	TSoftObjectPtr<UDataTable> TextTable;

	UPROPERTY(Config, EditAnywhere, Category="Localization")
	TSoftObjectPtr<UFont> CjkFont;

	UPROPERTY(Config, EditAnywhere, Category="Localization")
	TSoftObjectPtr<UFont> LatinFont;

	UPROPERTY(Config, EditAnywhere, Category="Settings")
	TSoftClassPtr<UGameSettingsWidget> SettingsWidgetClass;
};
