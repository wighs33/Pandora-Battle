#pragma once

#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"

#include "RightSkinWidget.generated.h"

class UButton;
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

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|Skin")
	TArray<TObjectPtr<UButton>> FilterButtonList;

	//------------------------------------------------------------------------------------------------------------------
	//--- Tile View
	UPROPERTY(BlueprintReadOnly, Category = "!UI|Skin", meta = (BindWidget))
	TObjectPtr<UTileView> TileView;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Skin|Slots", meta = (ClampMin = "0"))
	int32 SkinSlotCount = 40;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|Skin|Slots")
	TArray<TObjectPtr<USkinSlotViewData>> CachedSlotViewData;

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

	void RebuildFilterButtonList();
	FGameplayTag GetPandoraTypeTag() const;
	FGameplayTag GetCosmeticsTypeTag() const;
	FGameplayTag GetGestureTypeTag() const;
	FGameplayTag GetRidingTypeTag() const;
};
