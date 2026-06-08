#pragma once

#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"
#include "SkinSlotWidget.generated.h"

class USkinInstance;
class UImage;
class USkinSlotViewData;
class UTextBlock;
class UDragItemVisualWidget;
class UDragDropOperation;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API USkinSlotWidget : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "!UI|Skin")
	void SetData(USkinInstance* Target);

	UFUNCTION(BlueprintCallable, Category = "!UI|Skin")
	void SetSlotData(USkinSlotViewData* Target);

	UFUNCTION(BlueprintCallable, Category = "!UI|Skin")
	void SetSelected(bool bInSelected);

	UFUNCTION(BlueprintPure, Category = "!UI|Skin")
	bool IsSelected() const { return bIsSelected; }

	UFUNCTION(BlueprintPure, Category = "!UI|Skin")
	USkinInstance* GetCachedData() const { return CachedData; }

	UFUNCTION(BlueprintPure, Category = "!UI|Skin")
	USkinSlotViewData* GetCachedSlotData() const { return CachedSlotData; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Skin|Style")
	FLinearColor SelectionBorderDefaultColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.35f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Skin|Style")
	FLinearColor SelectionBorderSelectedColor = FLinearColor(0.0f, 0.45f, 1.0f, 1.0f);

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	virtual void NativeOnItemSelectionChanged(bool bIsSelected) override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Skin|Bind")
	TObjectPtr<UTextBlock> TextBlock;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Skin|Bind")
	TObjectPtr<UImage> IconImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Skin|Bind")
	TObjectPtr<UImage> SelectionBorderImage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Skin|DragDrop")
	TSubclassOf<UDragItemVisualWidget> DragVisualWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Skin|DragDrop")
	FVector2D DragIconSize = FVector2D(56.0f, 56.0f);

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|Skin")
	TObjectPtr<USkinInstance> CachedData;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|Skin")
	TObjectPtr<USkinSlotViewData> CachedSlotData;

private:
	void ApplySkinVisual(USkinInstance* Target);
	void CacheOptionalWidgets();
	void ApplySelectionVisual();

	bool bIsSelected = false;
};
