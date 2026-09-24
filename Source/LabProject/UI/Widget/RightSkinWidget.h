#pragma once

#include "CoreMinimal.h"
#include "UI/Widget/LocalizedMenuWidget.h"
#include "GameplayTagContainer.h"
#include "UI/Widget/FilterButtonHighlight.h"

#include "RightSkinWidget.generated.h"

class UButton;
class UEditableTextBox;
class USkinEquipmentComponent;
class USkinDefinition;
class USkinSlotViewData;
class UTileView;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPdOnClickedSkinFilterAllButton);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPdOnClickedSkinFilterTypeButton, FGameplayTag, TypeTag);

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API URightSkinWidget : public ULocalizedMenuWidget
{
	GENERATED_BODY()

protected:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

public:
	// Public API ------------------------------------------------------------------------------------------------------
	URightSkinWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "!UI|Skin")
	void SelectAllFilter();

	UFUNCTION(BlueprintCallable, Category = "!UI|Skin", meta = (Categories = "Skin"))
	void SelectTypeFilter(FGameplayTag TypeTag);

	UFUNCTION(BlueprintCallable, Category = "!UI|Skin")
	void ToggleActiveFiliterButtons(bool bActive);

	UFUNCTION(BlueprintCallable, Category = "!UI|Skin")
	void ResetFilterHighlightToAll();

	UFUNCTION(BlueprintCallable, Category = "!UI|Skin")
	void SetTileView(const TArray<UObject*>& InListItems);

	UFUNCTION(BlueprintCallable, Category = "!UI|Skin")
	void ClearTileViewItemClicked();

	UFUNCTION(BlueprintPure, Category = "!UI|Skin")
	UTileView* GetTileView() const { return TileView; }

	UFUNCTION(BlueprintPure, Category = "!UI|Skin")
	int32 GetSkinSlotCount() const { return SkinSlotCount; }

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	UFUNCTION()
	void OnAllButtonClicked();

	UFUNCTION()
	void OnPandoraButtonClicked();

	UFUNCTION()
	void OnCosmeticsButtonClicked();

	UFUNCTION()
	void OnGestureButtonClicked();

	UFUNCTION()
	void OnRidingButtonClicked();

	UFUNCTION()
	void OnPetButtonClicked();

	UFUNCTION()
	void OnSearchButtonClicked();

	UFUNCTION()
	void HandleEquippedSkinsChanged();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void RebuildFilterButtonList();
	void RebuildTileViewFromCachedSourceItems();
	void RefreshSkinEquipmentBinding();
	void ClearSkinEquipmentBinding();
	bool DoesSkinMatchSearch(const USkinDefinition* SkinDefinition, const FString& SearchText) const;
	void ApplyWidgetDefinitionSettings();
	UButton* ResolveFilterButton(FGameplayTag TypeTag) const;
	FGameplayTag GetPandoraTypeTag() const;
	FGameplayTag GetCosmeticsTypeTag() const;
	FGameplayTag GetGestureTypeTag() const;
	FGameplayTag GetRidingTypeTag() const;
	FGameplayTag GetPetTypeTag() const;

public:
	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "!UI|Skin")
	FPdOnClickedSkinFilterAllButton OnClicked_SkinFilterAllButton;

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "!UI|Skin")
	FPdOnClickedSkinFilterTypeButton OnClicked_SkinFilterTypeButton;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "!UI|Skin", meta = (BindWidget))
	TObjectPtr<UButton> AllButton;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Skin", meta = (BindWidget))
	TObjectPtr<UButton> PandoraButton;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Skin", meta = (BindWidget))
	TObjectPtr<UButton> CosmeticsButton;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Skin", meta = (BindWidget))
	TObjectPtr<UButton> GestureButton;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Skin", meta = (BindWidget))
	TObjectPtr<UButton> RidingButton;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Skin", meta = (BindWidgetOptional))
	TObjectPtr<UButton> PetButton;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|Skin")
	TArray<TObjectPtr<UButton>> FilterButtonList;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Skin|Filter")
	FLinearColor SelectedFilterAccentColor = FLinearColor(0.0f, 0.45f, 1.0f, 1.0f);

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Skin", meta = (BindWidget))
	TObjectPtr<UTileView> TileView;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Skin|Search", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Search;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Skin|Search", meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> SearchBox;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Skin|Slots", meta = (ClampMin = "0"))
	int32 SkinSlotCount = 40;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|Skin|Slots")
	TArray<TObjectPtr<USkinSlotViewData>> CachedSlotViewData;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> CachedSourceListItems;

	UPROPERTY(Transient)
	FString ActiveSearchText;

private:
	FGameplayTag PandoraTypeTagOverride;
	FGameplayTag CosmeticsTypeTagOverride;
	FGameplayTag GestureTypeTagOverride;
	FGameplayTag RidingTypeTagOverride;
	FGameplayTag PetTypeTagOverride;

	FFilterButtonHighlightState FilterButtonHighlightState;
	TWeakObjectPtr<USkinEquipmentComponent> BoundSkinEquipmentComponent;
};
