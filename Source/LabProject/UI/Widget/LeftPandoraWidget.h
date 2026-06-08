#pragma once

#include "Blueprint/UserWidget.h"
#include "UI/Widget/PandoraEquipSlotWidget.h"
#include "LeftPandoraWidget.generated.h"

class UPandoraComponent;
class UPandoraInstance;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FPdOnClickedPandoraEquipSlot,
	UPandoraEquipSlotWidget*, SelectedPandoraEquipSlot,
	bool, bIsSelectedAnyButton);

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API ULeftPandoraWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void ToggleActiveEquipSlots(bool bActive);

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void SelectPandoraEquipSlot(UPandoraEquipSlotWidget* InSelectedPandoraEquipSlot);

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void RefreshPandoraLoadoutSlots(const UPandoraComponent* PandoraComponent);

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "!UI|Pandora")
	FPdOnClickedPandoraEquipSlot OnClicked_PandoraEquipSlot;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Pandora|Bind")
	TObjectPtr<UPandoraEquipSlotWidget> FirstPandora;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Pandora|Bind")
	TObjectPtr<UPandoraEquipSlotWidget> SecondPandora;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Pandora|Bind")
	TObjectPtr<UPandoraEquipSlotWidget> ThirdPandora;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|Pandora")
	TArray<TObjectPtr<UPandoraEquipSlotWidget>> PandoraEquipSlotList;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|Pandora")
	TObjectPtr<UPandoraEquipSlotWidget> SelectedPandoraEquipSlot;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|Pandora", meta = (DisplayName = "IsSelectedAnyButton?"))
	bool bIsSelectedAnyButton = false;

private:
	UFUNCTION()
	void HandlePandoraEquipSlotClicked(UPandoraEquipSlotWidget* PandoraEquipSlot);

	void RebuildPandoraEquipSlotList();
	void BindPandoraEquipSlotCallbacks();
	void UnbindPandoraEquipSlotCallbacks();
};
