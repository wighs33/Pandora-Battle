#pragma once

#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "UI/Widget/EquipSlotWidget.h"
#include "LeftEquipmentWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FPdOnClickedEquipTypeSlot,
	FGameplayTag, EquipTypeTag,
	UEquipSlotWidget*, SelectedEquipSlot,
	bool, bIsSelectedAnyButton);

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API ULeftEquipmentWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	ULeftEquipmentWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "!UI|Equipment")
	void InitialzeEquipSlots();

	UFUNCTION(BlueprintCallable, Category = "!UI|Equipment")
	void ToggleActiveEquipSlots(bool bActive);

	UFUNCTION(BlueprintCallable, Category = "!UI|Equipment")
	void SelectEquipSlot(FGameplayTag EquipTypeTag, UEquipSlotWidget* InSelectedEquipSlot);

	UFUNCTION(BlueprintCallable, Category = "!UI|Equipment", meta = (Categories = "Item"))
	void BroadcastClickedEquipTypeSlot(FGameplayTag EquipTypeTag, UEquipSlotWidget* InSelectedEquipSlot, bool bInIsSelectedAnyButton);

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "!UI|Equipment")
	FPdOnClickedEquipTypeSlot OnClicked_EquipTypeSlot;

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Equipment|Bind")
	TObjectPtr<UEquipSlotWidget> HatSlot;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Equipment|Bind")
	TObjectPtr<UEquipSlotWidget> TopSlot;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Equipment|Bind")
	TObjectPtr<UEquipSlotWidget> BottomSlot;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Equipment|Bind")
	TObjectPtr<UEquipSlotWidget> ShoesSlot;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Equipment|Bind")
	TObjectPtr<UEquipSlotWidget> EarringSlot;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Equipment|Bind")
	TObjectPtr<UEquipSlotWidget> NecklaceSlot;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Equipment|Bind")
	TObjectPtr<UEquipSlotWidget> RingSlot;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Equipment|Bind")
	TObjectPtr<UEquipSlotWidget> RuneSlot;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Equipment|Bind")
	TObjectPtr<UEquipSlotWidget> QuickSlot1;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Equipment|Bind")
	TObjectPtr<UEquipSlotWidget> QuickSlot2;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Equipment|Bind")
	TObjectPtr<UEquipSlotWidget> QuickSlot3;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Equipment|Bind")
	TObjectPtr<UEquipSlotWidget> QuickSlot4;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Equipment|Bind")
	TObjectPtr<UEquipSlotWidget> ToolSlot;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Equipment|Bind")
	TObjectPtr<UEquipSlotWidget> Weapon1;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Equipment|Bind")
	TObjectPtr<UEquipSlotWidget> Weapon2;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Equipment|Bind")
	TObjectPtr<UEquipSlotWidget> Weapon3;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|Equipment")
	TArray<TObjectPtr<UEquipSlotWidget>> EquipSlotList;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|Equipment")
	TArray<FText> EquipSlotNameList;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|Equipment")
	TObjectPtr<UEquipSlotWidget> SelectedEquipSlot;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|Equipment", meta = (DisplayName = "IsSelectedAnyButton?"))
	bool bIsSelectedAnyButton = false;

private:
	UFUNCTION()
	void HandleEquipSlotClicked(UEquipSlotWidget* ItemSlot);

	void RebuildEquipSlotList();
	void RebuildEquipSlotNameList();
	void ApplyEquipSlotNames();
	void BindEquipSlotCallbacks();
	void UnbindEquipSlotCallbacks();
	FGameplayTag ResolveEquipTypeTagForSlot(const UEquipSlotWidget* ItemSlot) const;
};
