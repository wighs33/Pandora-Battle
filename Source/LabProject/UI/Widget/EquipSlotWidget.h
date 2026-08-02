#pragma once

#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "Styling/SlateTypes.h"
#include "EquipSlotWidget.generated.h"

class UButton;
class UImage;
class UItemInstance;
class UTextBlock;
class UTexture2D;
class UEquipSlotWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPdOnClickedEquipSlot, UEquipSlotWidget*, ItemSlot);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPdOnDroppedItemEquipSlot, UEquipSlotWidget*, EquipSlot, UItemInstance*, ItemInstance);

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UEquipSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "!UI|Equipment")
	void BroadcastClickedEquipSlot(UEquipSlotWidget* ItemSlot);

	UFUNCTION(BlueprintCallable, Category = "!UI|Equipment")
	void SetText(const FText& InText);

	UFUNCTION(BlueprintCallable, Category = "!UI|Equipment")
	void SetIcon(UTexture2D* InIconTexture);

	UFUNCTION(BlueprintCallable, Category = "!UI|Equipment")
	void SetHoverIcon(UTexture2D* InIconTexture);

	UFUNCTION(BlueprintCallable, Category = "!UI|Equipment|Pandora")
	void SetPandoraWeaponRequirementIcon(
		UTexture2D* InIconTexture,
		float InOpacity = 0.3f);

	UFUNCTION(BlueprintCallable, Category = "!UI|Equipment")
	void SetData(UItemInstance* Target);

	UFUNCTION(BlueprintCallable, Category = "!UI|Equipment")
	void SetSelected(bool bInSelected);

	UFUNCTION(BlueprintPure, Category = "!UI|Equipment")
	bool IsSelected() const { return bIsSelected; }

	UFUNCTION(BlueprintPure, Category = "!UI|Equipment")
	FText GetSlotText() const { return SlotText; }

	UFUNCTION(BlueprintPure, Category = "!UI|Equipment")
	UTexture2D* GetSlotIconTexture() const { return SlotIconTexture; }

	UFUNCTION(BlueprintPure, Category = "!UI|Equipment")
	UTexture2D* GetSlotHoverIconTexture() const { return SlotHoverIconTexture; }

	UFUNCTION(BlueprintPure, Category = "!UI|Equipment")
	int32 GetNth() const { return Nth; }

	UFUNCTION(BlueprintPure, Category = "!UI|Equipment")
	UItemInstance* GetItemInstance() const { return ItemInstance; }

	UFUNCTION(BlueprintPure, Category = "!UI|Equipment")
	bool HasEquippedItem() const { return ItemInstance != nullptr; }

	UFUNCTION(BlueprintPure, Category = "!UI|Equipment", meta = (Categories = "Item"))
	FGameplayTag GetEquipTypeTag() const { return EquipTypeTag; }

	UFUNCTION(BlueprintCallable, Category = "!UI|Equipment", meta = (Categories = "Item"))
	void SetResolvedEquipTypeTag(FGameplayTag InResolvedEquipTypeTag);

	UFUNCTION(BlueprintPure, Category = "!UI|Equipment", meta = (Categories = "Item"))
	FGameplayTag GetAcceptedEquipTypeTag() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Equipment")
	int32 Nth = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Equipment", meta = (Categories = "Item"))
	FGameplayTag EquipTypeTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Equipment")
	TObjectPtr<UTexture2D> SlotIconTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Equipment")
	TObjectPtr<UTexture2D> SlotHoverIconTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Equipment|Style")
	FLinearColor SelectionBorderDefaultColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.35f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Equipment|Style")
	FLinearColor SelectionBorderSelectedColor = FLinearColor(0.0f, 0.45f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Equipment|Style")
	FLinearColor ButtonNormalColor = FLinearColor(0.55f, 0.85f, 0.38f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Equipment|Style")
	FLinearColor ButtonHoverColor = FLinearColor(1.0f, 0.92f, 0.1f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Equipment|Style")
	FLinearColor ButtonPressedColor = FLinearColor(0.72f, 0.66f, 0.08f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Equipment|Style", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ButtonBackgroundOpacity = 1.0f;

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "!UI|Equipment")
	FPdOnClickedEquipSlot OnClicked_EquipSlot;

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "!UI|Equipment")
	FPdOnDroppedItemEquipSlot OnDroppedItem_EquipSlot;

protected:
	virtual void NativeConstruct() override;
	virtual void NativePreConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Equipment|Bind")
	TObjectPtr<UButton> ItemButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Equipment|Bind")
	TObjectPtr<UTextBlock> ApplyText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Equipment|Bind")
	TObjectPtr<UTextBlock> QuantityTextBlock;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Equipment|Bind")
	TObjectPtr<UImage> IconImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Equipment|Bind")
	TObjectPtr<UImage> ItemImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Equipment|Bind")
	TObjectPtr<UImage> SelectionBorderImage;

private:
	UFUNCTION()
	void HandleButtonClicked();

	UFUNCTION()
	void HandleButtonHovered();

	UFUNCTION()
	void HandleButtonUnhovered();

	void ApplySlotVisual();
	void CacheOptionalWidgets();
	void ApplyButtonBackgroundStyle();
	bool CanAcceptDroppedItem(UItemInstance* DroppedItem) const;
	bool IsCurrentItemConsumable() const;
	UTexture2D* GetCurrentIconTexture(bool bForHover) const;
	void CacheDefaultButtonStyle();

	UPROPERTY(Transient)
	FText SlotText;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> CurrentIconTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> CurrentHoverIconTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> CurrentItemIconTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> PandoraWeaponRequirementIconTexture;

	UPROPERTY(Transient)
	TObjectPtr<UItemInstance> ItemInstance;

	UPROPERTY(Transient)
	FGameplayTag ResolvedEquipTypeTag;

	float PandoraWeaponRequirementIconOpacity = 0.3f;
	FButtonStyle DefaultButtonStyle;
	bool bHasDefaultButtonStyle = false;
	bool bIsButtonHovered = false;
	bool bIsAcceptedDragHovered = false;
	bool bUseSelectedEmptyIcon = false;
	bool bIsSelected = false;
};
