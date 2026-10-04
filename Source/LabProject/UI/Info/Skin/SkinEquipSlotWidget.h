#pragma once

#include "UI/Info/EquipSlotWidgetBase.h"
#include "SkinEquipSlotWidget.generated.h"

class USkinDefinition;
class USkinEquipSlotWidget;
class UWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPdOnClickedSkinEquipSlot, USkinEquipSlotWidget*, SkinEquipSlot);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPdOnDroppedSkinEquipSlot, USkinEquipSlotWidget*, SkinEquipSlot, const USkinDefinition*, SkinDefinition);

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API USkinEquipSlotWidget : public UEquipSlotWidgetBase
{
	GENERATED_BODY()

protected:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void OnMenuLanguageChanged() override;

public:
	// Public API ------------------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "!UI|Skin")
	void BroadcastClickedSkinEquipSlot(USkinEquipSlotWidget* SkinEquipSlot);

	UFUNCTION(BlueprintCallable, Category = "!UI|Skin")
	void SetSkinDefinition(const USkinDefinition* Target);

	UFUNCTION(BlueprintPure, Category = "!UI|Skin")
	const USkinDefinition* GetSkinDefinition() const { return SkinDefinition; }

	UFUNCTION(BlueprintPure, Category = "!UI|Skin")
	bool HasEquippedSkin() const { return SkinDefinition != nullptr; }

	/** 제스처 슬롯은 목록이 정해 준 칸 번호 태그를 우선한다. */
	virtual FGameplayTag GetAcceptedEquipTypeTag() const override;

protected:
	virtual bool HasSlotContent() const override { return SkinDefinition != nullptr; }
	virtual bool ShowSlotDetail(UInfoWidget& InfoWidget) override;
	virtual bool CanAcceptDragOperation(UDragDropOperation* Operation) const override;
	virtual void BroadcastAcceptedDrop(UDragDropOperation* Operation) override;
	virtual void BroadcastSlotClicked() override;
	virtual void ApplySlotVisual() override;

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	void ApplyButtonBackgroundStyle();
	bool CanAcceptDroppedSkin(const USkinDefinition* DroppedSkin) const;

public:
	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "!UI|Skin")
	FPdOnClickedSkinEquipSlot OnClicked_SkinEquipSlot;

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "!UI|Skin")
	FPdOnDroppedSkinEquipSlot OnDroppedSkin_SkinEquipSlot;

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Skin|Bind")
	TObjectPtr<UImage> SkinImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Skin|Bind")
	TObjectPtr<UWidget> AssignedBadgeRoot;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> CurrentSkinIconTexture;

	UPROPERTY(Transient)
	TObjectPtr<const USkinDefinition> SkinDefinition;
};
