#pragma once

#include "Blueprint/UserWidget.h"
#include "Styling/SlateTypes.h"
#include "PandoraEquipSlotWidget.generated.h"

class UButton;
class UPandoraInstance;
class UTextBlock;
class UPandoraEquipSlotWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPdOnClickedPandoraEquipSlotWidget, UPandoraEquipSlotWidget*, PandoraEquipSlot);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPdOnHoveredPandoraEquipSlotWidget, UPandoraEquipSlotWidget*, PandoraEquipSlot);

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UPandoraEquipSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void BroadcastClickedPandoraEquipSlot(UPandoraEquipSlotWidget* PandoraEquipSlot);

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void BroadcastHoveredPandoraEquipSlot(UPandoraEquipSlotWidget* PandoraEquipSlot);

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void SetData(UPandoraInstance* Target);

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void ToggleText_Apply(bool bOn);

	UFUNCTION(BlueprintPure, Category = "!UI|Pandora")
	UPandoraInstance* GetCachedData() const { return CachedData; }

	UFUNCTION(BlueprintPure, Category = "!UI|Pandora")
	int32 GetNth() const { return Nth; }

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "!UI|Pandora")
	FPdOnClickedPandoraEquipSlotWidget OnClicked_PandoraEquipSlot;

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "!UI|Pandora")
	FPdOnHoveredPandoraEquipSlotWidget OnHovered_PandoraEquipSlot;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Pandora|Bind")
	TObjectPtr<UButton> ItemButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Pandora|Bind")
	TObjectPtr<UTextBlock> ApplyText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Pandora")
	int32 Nth = 0;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|Pandora")
	TObjectPtr<UPandoraInstance> CachedData;

private:
	UFUNCTION()
	void HandleButtonClicked();

	UFUNCTION()
	void HandleButtonHovered();

	void ApplyButtonStyle();

	FButtonStyle DefaultButtonStyle;
	bool bHasDefaultButtonStyle = false;
};
