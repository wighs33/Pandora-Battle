#pragma once

#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "UI/Widget/PandoraEquipSlotWidget.h"
#include "LeftPandoraWidget.generated.h"

class UImage;
class UPandoraComponent;
class UPandoraInstance;
class UPandoraTreeComponent;
class UTextBlock;
class UTexture2D;

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
	void ClearPandoraEquipSlotSelection();

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void RefreshPandoraLoadoutSlots(const UPandoraComponent* PandoraComponent);

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void SetWeaponImage(int32 Nth, UTexture2D* WeaponImage);

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

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Pandora|Bind")
	TObjectPtr<UTextBlock> FirstPandoraLevel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Pandora|Bind")
	TObjectPtr<UTextBlock> SecondPandoraLevel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Pandora|Bind")
	TObjectPtr<UTextBlock> ThirdPandoraLevel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Pandora|Bind")
	TObjectPtr<UTextBlock> Txt1;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Pandora|Bind")
	TObjectPtr<UTextBlock> Txt2;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Pandora|Bind")
	TObjectPtr<UTextBlock> Txt3;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Pandora|Bind")
	TObjectPtr<UImage> FirstWeaponImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Pandora|Bind")
	TObjectPtr<UImage> SecondWeaponImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Pandora|Bind")
	TObjectPtr<UImage> ThirdWeaponImage;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|Pandora")
	TArray<TObjectPtr<UPandoraEquipSlotWidget>> PandoraEquipSlotList;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|Pandora")
	TObjectPtr<UPandoraEquipSlotWidget> SelectedPandoraEquipSlot;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|Pandora", meta = (DisplayName = "IsSelectedAnyButton?"))
	bool bIsSelectedAnyButton = false;

private:
	UFUNCTION()
	void HandlePandoraEquipSlotClicked(UPandoraEquipSlotWidget* PandoraEquipSlot);

	UFUNCTION()
	void HandlePandoraLoadoutChanged();

	UFUNCTION()
	void HandlePandoraTreeChanged();

	UPandoraComponent* ResolveOwningPandoraComponent() const;
	UPandoraTreeComponent* ResolvePandoraTreeComponent(const UPandoraComponent* PandoraComponent) const;
	void BindPandoraLoadoutChanged();
	void BindPandoraLoadoutChanged(UPandoraComponent* PandoraComponent);
	void UnbindPandoraLoadoutChanged();
	void BindPandoraTreeChanged(UPandoraTreeComponent* PandoraTreeComponent);
	void UnbindPandoraTreeChanged();
	void CacheDefaultWeaponImageBrushes();
	void RebuildPandoraEquipSlotList();
	void BindPandoraEquipSlotCallbacks();
	void UnbindPandoraEquipSlotCallbacks();

	UPROPERTY(Transient)
	TObjectPtr<UPandoraComponent> BoundPandoraComponent;

	UPROPERTY(Transient)
	TObjectPtr<UPandoraTreeComponent> BoundPandoraTreeComponent;

	UPROPERTY(Transient)
	TArray<FSlateBrush> DefaultWeaponImageBrushes;

	UPROPERTY(Transient)
	bool bDefaultWeaponImageBrushesCached = false;
};
