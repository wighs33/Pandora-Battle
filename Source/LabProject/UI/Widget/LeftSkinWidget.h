#pragma once

#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "UI/Widget/SkinEquipSlotWidget.h"
#include "LeftSkinWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FPdOnClickedSkinEquipTypeSlot,
	FGameplayTag, EquipTypeTag,
	USkinEquipSlotWidget*, SelectedSkinEquipSlot,
	bool, bIsSelectedAnySlot);

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API ULeftSkinWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	ULeftSkinWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "!UI|Skin")
	void InitialzeEquipSlots();

	UFUNCTION(BlueprintCallable, Category = "!UI|Skin")
	void ToggleActiveSkinEquipSlots(bool bActive);

	UFUNCTION(BlueprintCallable, Category = "!UI|Skin", meta = (Categories = "Skin"))
	void SelectSkinEquipSlot(FGameplayTag EquipTypeTag, USkinEquipSlotWidget* InSelectedSkinEquipSlot);

	UFUNCTION(BlueprintCallable, Category = "!UI|Skin", meta = (Categories = "Skin"))
	void BroadcastClickedSkinEquipTypeSlot(FGameplayTag EquipTypeTag, USkinEquipSlotWidget* InSelectedSkinEquipSlot, bool bInIsSelectedAnySlot);

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "!UI|Skin")
	FPdOnClickedSkinEquipTypeSlot OnClicked_SkinEquipTypeSlot;

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Skin|Bind")
	TObjectPtr<USkinEquipSlotWidget> HatSlot;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Skin|Bind")
	TObjectPtr<USkinEquipSlotWidget> TopSlot;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Skin|Bind")
	TObjectPtr<USkinEquipSlotWidget> BottomSlot;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Skin|Bind")
	TObjectPtr<USkinEquipSlotWidget> ShoesSlot;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Skin|Bind")
	TObjectPtr<USkinEquipSlotWidget> HairSlot;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Skin|Bind")
	TObjectPtr<USkinEquipSlotWidget> FaceSlot;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Skin|Bind")
	TObjectPtr<USkinEquipSlotWidget> BackSlot;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Skin|Bind")
	TObjectPtr<USkinEquipSlotWidget> AuraSlot;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Skin|Bind")
	TObjectPtr<USkinEquipSlotWidget> GestureSlot1;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Skin|Bind")
	TObjectPtr<USkinEquipSlotWidget> GestureSlot2;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Skin|Bind")
	TObjectPtr<USkinEquipSlotWidget> GestureSlot3;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Skin|Bind")
	TObjectPtr<USkinEquipSlotWidget> GestureSlot4;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Skin|Bind")
	TObjectPtr<USkinEquipSlotWidget> RidingSlot;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|Skin")
	TArray<TObjectPtr<USkinEquipSlotWidget>> SkinEquipSlotList;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|Skin")
	TArray<FText> EquipSlotNameList;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|Skin")
	TObjectPtr<USkinEquipSlotWidget> SelectedSkinEquipSlot;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|Skin", meta = (DisplayName = "IsSelectedAnySlot?"))
	bool bIsSelectedAnySlot = false;

private:
	UFUNCTION()
	void HandleSkinEquipSlotClicked(USkinEquipSlotWidget* SkinEquipSlot);

	void RebuildSkinEquipSlotList();
	void RebuildEquipSlotNameList();
	void ApplyEquipSlotNames();
	void BindSkinEquipSlotCallbacks();
	void UnbindSkinEquipSlotCallbacks();

	FGameplayTag ResolveSkinEquipTypeTagForSlot(const USkinEquipSlotWidget* SkinEquipSlot) const;
};
