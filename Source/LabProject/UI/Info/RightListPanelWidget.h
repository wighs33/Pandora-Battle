#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UI/Common/LocalizedMenuWidget.h"
#include "UI/Info/FilterButtonHighlight.h"
#include "RightListPanelWidget.generated.h"

class UButton;
class UButtonClickRelay;
class UEditableTextBox;
class UProjectTagDefinition;
class UTileView;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPdOnClickedFilterAllButton);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPdOnClickedFilterTypeButton, FGameplayTag, TypeTag);

/**
 * Info 화면 오른쪽 목록 패널(아이템·스킨·판도라)의 공통 흐름.
 * 전체·분류 필터 버튼의 선택 표시와 알림, 검색어 적용, 원본 목록 보관을 맡는다.
 * 분류 버튼마다 쓸 태그와, 원본 목록을 타일 항목으로 바꾸는 방법은 파생 클래스가 정한다.
 */
UCLASS(Abstract)
class LABPROJECT_API URightListPanelWidget : public ULocalizedMenuWidget
{
	GENERATED_BODY()

protected:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

public:
	// Public API ------------------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "!UI|Info|List")
	void SelectAllFilter();

	UFUNCTION(BlueprintCallable, Category = "!UI|Info|List")
	void SelectTypeFilter(FGameplayTag TypeTag);

	UFUNCTION(BlueprintCallable, Category = "!UI|Info|List")
	void SetFilterButtonsEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "!UI|Info|List")
	void ResetFilterHighlightToAll();

	/** 원본 목록을 보관하고 현재 검색어로 타일 목록을 다시 만든다. */
	UFUNCTION(BlueprintCallable, Category = "!UI|Info|List")
	void SetTileView(const TArray<UObject*>& InListItems);

	UFUNCTION(BlueprintCallable, Category = "!UI|Info|List")
	void ClearTileViewItemClicked();

	UFUNCTION(BlueprintPure, Category = "!UI|Info|List")
	UTileView* GetTileView() const { return TileView; }

protected:
	using FDefaultTypeTagGetter = const FGameplayTag& (UProjectTagDefinition::*)() const;

	/** 위젯 정의 설정을 읽고, AddTypeFilter로 분류 버튼을 화면 순서대로 등록한다. */
	virtual void ApplyWidgetDefinitionSettings() PURE_VIRTUAL(URightListPanelWidget::ApplyWidgetDefinitionSettings, );

	/** CachedSourceListItems와 ActiveSearchText로 타일 목록을 다시 만든다. */
	virtual void RebuildTileView() PURE_VIRTUAL(URightListPanelWidget::RebuildTileView, );

	// Internal Helpers ------------------------------------------------------------------------------------------------
	/** 설정 태그가 비어 있으면 프로젝트 태그 정의의 기본 태그를 쓴다. 등록한 순서가 분류 번호가 된다. */
	void AddTypeFilter(UButton* Button, FGameplayTag TagOverride, FDefaultTypeTagGetter DefaultTag);
	FGameplayTag GetTypeFilterTag(int32 FilterIndex) const;

	bool IsSearching() const { return !ActiveSearchText.IsEmpty(); }

	/** 현지화된 이름이나 애셋 이름에 검색어가 들어 있는지. 검색어가 비어 있으면 모두 통과한다. */
	bool MatchesSearch(const UObject* Product, const FText& DisplayName, const FString& SearchText) const;

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	UFUNCTION()
	void HandleAllFilterClicked();

	UFUNCTION()
	void HandleSearchClicked();

	void HandleTypeFilterClicked(int32 FilterIndex);

public:
	UPROPERTY(BlueprintAssignable, Category = "!UI|Info|List")
	FPdOnClickedFilterAllButton OnClicked_FilterAllButton;

	UPROPERTY(BlueprintAssignable, Category = "!UI|Info|List")
	FPdOnClickedFilterTypeButton OnClicked_FilterTypeButton;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info|List", meta = (BindWidget))
	TObjectPtr<UButton> AllButton;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|Info|List")
	TArray<TObjectPtr<UButton>> FilterButtonList;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Info|List|Filter")
	FLinearColor SelectedFilterAccentColor = FLinearColor(0.0f, 0.45f, 1.0f, 1.0f);

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info|List", meta = (BindWidget))
	TObjectPtr<UTileView> TileView;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info|List|Search", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Search;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info|List|Search", meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> SearchBox;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> CachedSourceListItems;

	/** 앞뒤 공백을 지운 검색어. 비어 있으면 검색하지 않는다. */
	UPROPERTY(Transient)
	FString ActiveSearchText;

private:
	struct FTypeFilter
	{
		TWeakObjectPtr<UButton> Button;
		FGameplayTag TagOverride;
		FDefaultTypeTagGetter DefaultTag = nullptr;
	};
	TArray<FTypeFilter> TypeFilters;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButtonClickRelay>> TypeFilterClickRelays;

	FFilterButtonHighlightState FilterButtonHighlightState;
};
