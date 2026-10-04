#pragma once

#include "UI/Info/EquipSlotWidgetBase.h"
#include "EquipSlotWidget.generated.h"

class UItemInstance;
class UEquipSlotWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPdOnClickedEquipSlot, UEquipSlotWidget*, ItemSlot);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPdOnDroppedItemEquipSlot, UEquipSlotWidget*, EquipSlot, UItemInstance*, ItemInstance);

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UEquipSlotWidget : public UEquipSlotWidgetBase
{
	GENERATED_BODY()

protected:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void OnMenuLanguageChanged() override;

public:
	// Public API ------------------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "!UI|Equipment")
	void BroadcastClickedEquipSlot(UEquipSlotWidget* ItemSlot);

	UFUNCTION(BlueprintCallable, Category = "!UI|Equipment|Pandora")
	void SetPandoraWeaponRequirementIcon(
		UTexture2D* InIconTexture,
		float InOpacity = 0.3f);

	UFUNCTION(BlueprintCallable, Category = "!UI|Equipment")
	void SetData(UItemInstance* Target);

	UFUNCTION(BlueprintPure, Category = "!UI|Equipment")
	int32 GetNth() const { return Nth; }

	UFUNCTION(BlueprintPure, Category = "!UI|Equipment")
	UItemInstance* GetItemInstance() const { return ItemInstance; }

	UFUNCTION(BlueprintPure, Category = "!UI|Equipment")
	bool HasEquippedItem() const { return ItemInstance != nullptr; }

protected:
	virtual bool HasSlotContent() const override { return ItemInstance != nullptr; }
	virtual bool ShowSlotDetail(UInfoWidget& InfoWidget) override;
	virtual bool CanAcceptDragOperation(UDragDropOperation* Operation) const override;
	virtual void BroadcastAcceptedDrop(UDragDropOperation* Operation) override;
	virtual void BroadcastSlotClicked() override;
	virtual void ApplySlotVisual() override;

	/** 판도라 무기 조건 아이콘이 있으면 빈 슬롯 아이콘 대신 그것을 보여 준다. */
	virtual UTexture2D* GetCurrentIconTexture(bool bForHover) const override;

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	void ApplyButtonBackgroundStyle();
	bool CanAcceptDroppedItem(const UItemInstance* DroppedItem) const;
	bool IsCurrentItemConsumable() const;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Equipment")
	int32 Nth = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Equipment|Style", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ButtonBackgroundOpacity = 1.0f;

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "!UI|Equipment")
	FPdOnClickedEquipSlot OnClicked_EquipSlot;

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "!UI|Equipment")
	FPdOnDroppedItemEquipSlot OnDroppedItem_EquipSlot;

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Equipment|Bind")
	TObjectPtr<UTextBlock> QuantityTextBlock;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Equipment|Bind")
	TObjectPtr<UImage> ItemImage;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> CurrentItemIconTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> PandoraWeaponRequirementIconTexture;

	UPROPERTY(Transient)
	TObjectPtr<UItemInstance> ItemInstance;

	float PandoraWeaponRequirementIconOpacity = 0.3f;
};
