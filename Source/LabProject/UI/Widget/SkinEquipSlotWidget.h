#pragma once

#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "Styling/SlateTypes.h"
#include "SkinEquipSlotWidget.generated.h"

class UButton;
class UDragDropOperation;
class UImage;
class USkinDefinition;
class USkinInstance;
class USkinEquipSlotWidget;
class UTextBlock;
class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPdOnClickedSkinEquipSlot, USkinEquipSlotWidget*, SkinEquipSlot);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPdOnDroppedSkinEquipSlot, USkinEquipSlotWidget*, SkinEquipSlot, USkinInstance*, SkinInstance);

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API USkinEquipSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "!UI|Skin")
	void BroadcastClickedSkinEquipSlot(USkinEquipSlotWidget* SkinEquipSlot);

	UFUNCTION(BlueprintCallable, Category = "!UI|Skin")
	void SetText(const FText& InText);

	UFUNCTION(BlueprintCallable, Category = "!UI|Skin")
	void SetIcon(UTexture2D* InIconTexture);

	UFUNCTION(BlueprintCallable, Category = "!UI|Skin")
	void SetHoverIcon(UTexture2D* InIconTexture);

	UFUNCTION(BlueprintCallable, Category = "!UI|Skin")
	void SetData(USkinInstance* Target);

	UFUNCTION(BlueprintCallable, Category = "!UI|Skin")
	void SetSkinDefinition(const USkinDefinition* Target);

	UFUNCTION(BlueprintCallable, Category = "!UI|Skin")
	void SetSelected(bool bInSelected);

	UFUNCTION(BlueprintPure, Category = "!UI|Skin")
	bool IsSelected() const { return bIsSelected; }

	UFUNCTION(BlueprintPure, Category = "!UI|Skin")
	FText GetSlotText() const { return SlotText; }

	UFUNCTION(BlueprintPure, Category = "!UI|Skin")
	UTexture2D* GetSlotIconTexture() const { return SlotIconTexture; }

	UFUNCTION(BlueprintPure, Category = "!UI|Skin")
	UTexture2D* GetSlotHoverIconTexture() const { return SlotHoverIconTexture; }

	UFUNCTION(BlueprintPure, Category = "!UI|Skin")
	USkinInstance* GetSkinInstance() const { return SkinInstance; }

	UFUNCTION(BlueprintPure, Category = "!UI|Skin")
	const USkinDefinition* GetSkinDefinition() const { return SkinDefinition; }

	UFUNCTION(BlueprintPure, Category = "!UI|Skin")
	bool HasEquippedSkin() const { return SkinDefinition != nullptr; }

	UFUNCTION(BlueprintPure, Category = "!UI|Skin", meta = (Categories = "Skin"))
	FGameplayTag GetEquipTypeTag() const { return EquipTypeTag; }

	UFUNCTION(BlueprintCallable, Category = "!UI|Skin", meta = (Categories = "Skin"))
	void SetResolvedEquipTypeTag(FGameplayTag InResolvedEquipTypeTag);

	UFUNCTION(BlueprintPure, Category = "!UI|Skin", meta = (Categories = "Skin"))
	FGameplayTag GetAcceptedEquipTypeTag() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Skin", meta = (Categories = "Skin"))
	FGameplayTag EquipTypeTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Skin")
	TObjectPtr<UTexture2D> SlotIconTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Skin")
	TObjectPtr<UTexture2D> SlotHoverIconTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Skin|Style")
	FLinearColor SelectionBorderDefaultColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.35f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Skin|Style")
	FLinearColor SelectionBorderSelectedColor = FLinearColor(0.0f, 0.45f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Skin|Style")
	FLinearColor ButtonNormalColor = FLinearColor(0.55f, 0.85f, 0.38f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Skin|Style")
	FLinearColor ButtonHoverColor = FLinearColor(1.0f, 0.92f, 0.1f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Skin|Style")
	FLinearColor ButtonPressedColor = FLinearColor(0.72f, 0.66f, 0.08f, 1.0f);

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "!UI|Skin")
	FPdOnClickedSkinEquipSlot OnClicked_SkinEquipSlot;

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "!UI|Skin")
	FPdOnDroppedSkinEquipSlot OnDroppedSkin_SkinEquipSlot;

protected:
	virtual void NativeConstruct() override;
	virtual void NativePreConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Skin|Bind")
	TObjectPtr<UButton> ItemButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Skin|Bind")
	TObjectPtr<UTextBlock> ApplyText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Skin|Bind")
	TObjectPtr<UImage> IconImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Skin|Bind")
	TObjectPtr<UImage> SkinImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Skin|Bind")
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
	bool CanAcceptDroppedSkin(USkinInstance* DroppedSkin) const;
	UTexture2D* GetCurrentIconTexture(bool bForHover) const;
	void CacheDefaultButtonStyle();

	UPROPERTY(Transient)
	FText SlotText;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> CurrentIconTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> CurrentHoverIconTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> CurrentSkinIconTexture;

	UPROPERTY(Transient)
	TObjectPtr<USkinInstance> SkinInstance;

	UPROPERTY(Transient)
	TObjectPtr<const USkinDefinition> SkinDefinition;

	UPROPERTY(Transient)
	FGameplayTag ResolvedEquipTypeTag;

	FButtonStyle DefaultButtonStyle;
	bool bHasDefaultButtonStyle = false;
	bool bIsButtonHovered = false;
	bool bIsAcceptedDragHovered = false;
	bool bUseSelectedEmptyIcon = false;
	bool bIsSelected = false;
};
