#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UI/Presenter/InfoTabPresenterBase.h"
#include "InfoSkinTabPresenter.generated.h"

class USkinEquipSlotWidget;
class USkinInstance;

UCLASS()
class LABPROJECT_API UInfoSkinTabPresenter : public UInfoTabPresenterBase
{
	GENERATED_BODY()

public:
	virtual void BindInfoUi(UInfoWidget* InInfoWidget) override;
	virtual void Deinitialize() override;

	void Activate();
	void HandleInfoUiOpened();
	void RefreshEquippedSlots() const;

	UFUNCTION()
	void HandleSkinSlotClicked(UObject* Item);

	UFUNCTION()
	void HandleSkinEquipSlotClicked(
		FGameplayTag EquipTypeTag,
		USkinEquipSlotWidget* SelectedEquipSlot,
		bool bIsSelectedAnyButton);

	UFUNCTION()
	void HandleSkinEquipSlotDropped(
		FGameplayTag EquipTypeTag,
		USkinEquipSlotWidget* TargetSkinEquipSlot,
		USkinInstance* SkinInstance);

	UFUNCTION()
	void HandleSkinDroppedToCharacter(USkinInstance* SkinInstance);

	UFUNCTION()
	void HandleSkinFilterTypeClicked(FGameplayTag TypeTag);

	UFUNCTION()
	void HandleSkinFilterAllClicked();

private:
	void BindEvents();
	void UnbindEvents();
	void BindTileItemClicked();
	void ClearTileItemClicked() const;
	void ClearSkinEquipSlot(USkinEquipSlotWidget* TargetSkinEquipSlot, FGameplayTag EquipTypeTag);
	void PopulateAllSkins() const;

	UPROPERTY(Transient)
	TObjectPtr<USkinEquipSlotWidget> SelectedEquipSlot;

	UPROPERTY(Transient)
	FGameplayTag SelectedEquipTypeTag;
};
