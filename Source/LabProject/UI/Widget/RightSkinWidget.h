#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "UI/Widget/FilterButtonHighlight.h"

#include "RightSkinWidget.generated.h"

class UButton;
class UEditableTextBox;
class USkinEquipmentComponent;
class USkinInstance;
class USkinSlotViewData;
class UTileView;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPdOnClickedSkinFilterAllButton);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPdOnClickedSkinFilterTypeButton, FGameplayTag, TypeTag);

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API URightSkinWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	URightSkinWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//------------------------------------------------------------------------------------------------------------------
	//--- Filter
	UFUNCTION(BlueprintCallable, Category = "!UI|Skin")
	void SelectAllFilter();

	UFUNCTION(BlueprintCallable, Category = "!UI|Skin", meta = (Categories = "Skin"))
	void SelectTypeFilter(FGameplayTag TypeTag);

	UFUNCTION(BlueprintCallable, Category = "!UI|Skin")
	void ToggleActiveFiliterButtons(bool bActive);

	UFUNCTION(BlueprintCallable, Category = "!UI|Skin")
	void ResetFilterHighlightToAll();

	//------------------------------------------------------------------------------------------------------------------
	//--- Tile View
	UFUNCTION(BlueprintCallable, Category = "!UI|Skin")
	void SetTileView(const TArray<UObject*>& InListItems);

	UFUNCTION(BlueprintCallable, Category = "!UI|Skin")
	void ClearTileViewItemClicked();

	UFUNCTION(BlueprintPure, Category = "!UI|Skin")
	UTileView* GetTileView() const { return TileView; }

	UFUNCTION(BlueprintPure, Category = "!UI|Skin")
	int32 GetSkinSlotCount() const { return SkinSlotCount; }

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "!UI|Skin")
	FPdOnClickedSkinFilterAllButton OnClicked_SkinFilterAllButton;

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "!UI|Skin")
	FPdOnClickedSkinFilterTypeButton OnClicked_SkinFilterTypeButton;

protected:
	//------------------------------------------------------------------------------------------------------------------
	//--- Engine Callbacks
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	//------------------------------------------------------------------------------------------------------------------
	//--- Filter Buttons
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

	//------------------------------------------------------------------------------------------------------------------
	//--- Tile View
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
	//------------------------------------------------------------------------------------------------------------------
	//--- Button Callbacks
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

	void RebuildFilterButtonList();
	void RebuildTileViewFromCachedSourceItems();
	void RefreshSkinEquipmentBinding();
	void ClearSkinEquipmentBinding();
	bool DoesSkinMatchSearch(const USkinInstance* SkinInstance, const FString& SearchText) const;
	void ApplyWidgetDefinitionSettings();
	UButton* ResolveFilterButton(FGameplayTag TypeTag) const;
	FGameplayTag GetPandoraTypeTag() const;
	FGameplayTag GetCosmeticsTypeTag() const;
	FGameplayTag GetGestureTypeTag() const;
	FGameplayTag GetRidingTypeTag() const;
	FGameplayTag GetPetTypeTag() const;

	FGameplayTag PandoraTypeTagOverride;
	FGameplayTag CosmeticsTypeTagOverride;
	FGameplayTag GestureTypeTagOverride;
	FGameplayTag RidingTypeTagOverride;
	FGameplayTag PetTypeTagOverride;

	FFilterButtonHighlightState FilterButtonHighlightState;
	TWeakObjectPtr<USkinEquipmentComponent> BoundSkinEquipmentComponent;
};
