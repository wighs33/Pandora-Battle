#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Settings/UiLanguage.h"
#include "MenuLocalizationSubsystem.generated.h"

class UDataTable;
class UFont;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMenuLanguageChanged);

/** Owns local language selection, persistence and text lookup, independently of any screen. */
UCLASS()
class LABPROJECT_API UMenuLocalizationSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// Public API ------------------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintPure, Category="UI|Localization")
	EGuideLanguage GetLanguage() const { return Language; }

	UFUNCTION(BlueprintCallable, Category="UI|Localization")
	bool SetLanguage(EGuideLanguage NewLanguage, bool bSaveImmediately = true);

	UFUNCTION(BlueprintPure, Category="UI|Localization")
	FText GetText(FName Key) const;

	UFUNCTION(BlueprintPure, Category="UI|Localization")
	FText GetTextOrFallback(FName Key, const FText& Fallback) const;

	/** Stable product identity resolves text only; icons and gameplay data remain asset-owned. */
	UFUNCTION(BlueprintPure, Category="UI|Localization")
	FText GetProductText(const UObject* Product, FName Field, const FText& Fallback) const;

	UFUNCTION(BlueprintPure, Category="UI|Localization")
	UFont* GetFontForLanguage(EGuideLanguage FontLanguage) const;

public:
	UPROPERTY(BlueprintAssignable, Category="UI|Localization")
	FMenuLanguageChanged OnLanguageChanged;

private:
	UPROPERTY(Transient) TObjectPtr<UDataTable> TextTable;
	UPROPERTY(Transient) TObjectPtr<UFont> CjkFont;
	UPROPERTY(Transient) TObjectPtr<UFont> LatinFont;
	EGuideLanguage Language = EGuideLanguage::Korean;
};
