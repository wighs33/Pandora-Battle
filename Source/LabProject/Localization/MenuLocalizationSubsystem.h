#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Common/UiLanguage.h"
#include "MenuLocalizationSubsystem.generated.h"

class UDataTable;
class UFont;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMenuLanguageChanged);

/** 화면과 독립적으로 로컬 언어 선택·저장·텍스트 조회를 관리한다. */
UCLASS()
class LABPROJECT_API UMenuLocalizationSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// Public API ------------------------------------------------------------------------------------------------------
	EGuideLanguage GetLanguage() const { return Language; }

	bool SetLanguage(EGuideLanguage NewLanguage);

	FText GetText(FName Key) const;

	FText GetTextOrFallback(FName Key, const FText& Fallback) const;

	/** 상품 애셋 경로로 번역 문구만 찾는다. 아이콘과 게임플레이 데이터는 애셋이 가진 값을 쓴다. */
	FText GetProductText(const UObject* Product, FName Field, const FText& Fallback) const;

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
