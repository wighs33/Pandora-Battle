#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "UI/Widget/FilterButtonHighlight.h"
#include "RightPandoraWidget.generated.h"

class UButton;
class UEditableTextBox;
class UPandoraInstance;
class UTileView;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPdOnClickedPandoraFilterAllButton);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPdOnClickedPandoraFilterTypeButton, FGameplayTag, TypeTag);

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API URightPandoraWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	URightPandoraWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void SelectAllFilter();

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora", meta = (Categories = "Pandora"))
	void SelectTypeFilter(FGameplayTag TypeTag);

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void ToggleActiveFiliterButtons(bool bActive);

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void ResetFilterHighlightToAll();

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void SetTileViewAndShowLockState(const TArray<UObject*>& InListItems);

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void ClearTileViewItemClicked();

	UFUNCTION(BlueprintPure, Category = "!UI|Pandora")
	UTileView* GetTileView() const { return TileView; }

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "!UI|Pandora")
	FPdOnClickedPandoraFilterAllButton OnClicked_PandoraFilterAllButton;

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "!UI|Pandora")
	FPdOnClickedPandoraFilterTypeButton OnClicked_PandoraFilterTypeButton;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Pandora", meta = (BindWidget))
	TObjectPtr<UButton> AllButton;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Pandora", meta = (BindWidget))
	TObjectPtr<UButton> OffensiveButton;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Pandora", meta = (BindWidget))
	TObjectPtr<UButton> DefensiveButton;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Pandora", meta = (BindWidget))
	TObjectPtr<UButton> SupportButton;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Pandora", meta = (BindWidget))
	TObjectPtr<UButton> SpecialButton;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|Pandora")
	TArray<TObjectPtr<UButton>> FilterButtonList;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Pandora|Filter")
	FLinearColor SelectedFilterAccentColor = FLinearColor(0.0f, 0.45f, 1.0f, 1.0f);

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Pandora", meta = (BindWidget))
	TObjectPtr<UTileView> TileView;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Pandora|Search", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Search;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Pandora|Search", meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> SearchBox;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> CachedSourceListItems;

	UPROPERTY(Transient)
	FString ActiveSearchText;

private:
	UFUNCTION()
	void OnAllButtonClicked();

	UFUNCTION()
	void OnOffensiveButtonClicked();

	UFUNCTION()
	void OnDefensiveButtonClicked();

	UFUNCTION()
	void OnSupportButtonClicked();

	UFUNCTION()
	void OnSpecialButtonClicked();

	UFUNCTION()
	void OnSearchButtonClicked();

	void RebuildFilterButtonList();
	void RebuildTileViewFromCachedSourceItems();
	bool DoesPandoraMatchSearch(const UPandoraInstance* PandoraInstance, const FString& SearchText) const;
	void ApplyWidgetDefinitionSettings();
	UButton* ResolveFilterButton(FGameplayTag TypeTag) const;
	FGameplayTag GetOffensiveTypeTag() const;
	FGameplayTag GetDefensiveTypeTag() const;
	FGameplayTag GetSupportTypeTag() const;
	FGameplayTag GetSpecialTypeTag() const;

	FGameplayTag OffensiveTypeTagOverride;
	FGameplayTag DefensiveTypeTagOverride;
	FGameplayTag SupportTypeTagOverride;
	FGameplayTag SpecialTypeTagOverride;

	FFilterButtonHighlightState FilterButtonHighlightState;
};
