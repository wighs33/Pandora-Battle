#pragma once

#include "Blueprint/UserWidget.h"
#include "EquipSlotWidget.generated.h"

class UButton;
class UItemInstance;
class UTextBlock;
class UEquipSlotWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPdOnClickedEquipSlot, UEquipSlotWidget*, ItemSlot);

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
	void SetData(UItemInstance* Target);

	UFUNCTION(BlueprintPure, Category = "!UI|Equipment")
	FText GetSlotText() const { return SlotText; }

	UFUNCTION(BlueprintPure, Category = "!UI|Equipment")
	int32 GetNth() const { return Nth; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Equipment")
	int32 Nth = 0;

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "!UI|Equipment")
	FPdOnClickedEquipSlot OnClicked_EquipSlot;

protected:
	virtual void NativeConstruct() override;
	virtual void NativePreConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Equipment|Bind")
	TObjectPtr<UButton> ItemButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Equipment|Bind")
	TObjectPtr<UTextBlock> ApplyText;

private:
	UFUNCTION()
	void HandleButtonClicked();

	void ApplySlotText();

	UPROPERTY(Transient)
	FText SlotText;

	UPROPERTY(Transient)
	TObjectPtr<UItemInstance> ItemInstance;
};
