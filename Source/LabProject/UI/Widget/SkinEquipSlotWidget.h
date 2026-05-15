#pragma once

#include "Blueprint/UserWidget.h"
#include "SkinEquipSlotWidget.generated.h"

class UButton;
class USkinInstance;
class USkinEquipSlotWidget;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPdOnClickedSkinEquipSlot, USkinEquipSlotWidget*, SkinEquipSlot);

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
	void SetData(USkinInstance* Target);

	UFUNCTION(BlueprintPure, Category = "!UI|Skin")
	FText GetSlotText() const { return SlotText; }

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "!UI|Skin")
	FPdOnClickedSkinEquipSlot OnClicked_SkinEquipSlot;

protected:
	virtual void NativeConstruct() override;
	virtual void NativePreConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Skin|Bind")
	TObjectPtr<UButton> ItemButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Skin|Bind")
	TObjectPtr<UTextBlock> ApplyText;

private:
	UFUNCTION()
	void HandleButtonClicked();

	void ApplySlotText();

	UPROPERTY(Transient)
	FText SlotText;

	UPROPERTY(Transient)
	TObjectPtr<USkinInstance> SkinInstance;
};
