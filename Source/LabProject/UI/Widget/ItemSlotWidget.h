#pragma once

#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"
#include "UI/Widget/ItemViewData.h"
#include "ItemSlotWidget.generated.h"

class UItemInstance;
class UImage;
class UInventorySlotViewData;
class UDragItemVisualWidget;
class UTextBlock;
class UInventoryComponent;
class UDragDropOperation;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UItemSlotWidget : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "!UI|Inventory")
	void SetData(UItemInstance* Target);

	UFUNCTION(BlueprintCallable, Category = "!UI|Inventory")
	void SetSlotData(UInventorySlotViewData* Target);

	UFUNCTION(BlueprintCallable, Category = "!UI|Inventory")
	void SetSelected(bool bInSelected);

	UFUNCTION(BlueprintPure, Category = "!UI|Inventory")
	bool IsSelected() const { return bIsSelected; }

	UFUNCTION(BlueprintPure, Category = "!UI|Inventory")
	UItemInstance* GetCachedData() const { return CachedData; }

	UFUNCTION(BlueprintPure, Category = "!UI|Inventory")
	UInventorySlotViewData* GetCachedSlotData() const { return CachedSlotData; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Inventory|Style")
	FLinearColor SelectionBorderDefaultColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.35f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Inventory|Style")
	FLinearColor SelectionBorderSelectedColor = FLinearColor(0.0f, 0.45f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Inventory|Drag")
	TSubclassOf<UDragItemVisualWidget> DragVisualWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Inventory|Drag")
	FVector2D DragIconSize = FVector2D(56.0f, 56.0f);

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	virtual void NativeOnItemSelectionChanged(bool bIsSelected) override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Inventory|Bind")
	TObjectPtr<UTextBlock> TextBlock;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Inventory|Bind")
	TObjectPtr<UTextBlock> QuantityTextBlock;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Inventory|Bind")
	TObjectPtr<UImage> IconImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Inventory|Bind")
	TObjectPtr<UImage> SelectionBorderImage;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|Inventory")
	TObjectPtr<UItemInstance> CachedData;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|Inventory")
	TObjectPtr<UInventorySlotViewData> CachedSlotData;

private:
	void ApplyItemVisual(const FPdItemViewData& ViewData);
	void CacheOptionalWidgets();
	void ApplySelectionVisual();
	bool IsItemConsumable(const UItemInstance* ItemInstance) const;
	bool IsCachedItemConsumable() const;
	UInventoryComponent* ResolveOwningInventoryComponent() const;
	bool RequestSplitCachedStack() const;
	bool RequestMergeDraggedStack(UDragDropOperation* InOperation) const;

	UPROPERTY(Transient)
	FPdItemViewData CachedViewData;

	bool bIsSelected = false;
};
