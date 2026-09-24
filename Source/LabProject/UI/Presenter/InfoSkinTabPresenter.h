#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UI/Presenter/InfoTabPresenterBase.h"
#include "InfoSkinTabPresenter.generated.h"

class USkinEquipSlotWidget;
class USkinDefinition;
class USkinComponent;

UCLASS()
class LABPROJECT_API UInfoSkinTabPresenter : public UInfoTabPresenterBase
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	virtual void BindInfoUi(UInfoWidget* InInfoWidget) override;
	virtual void Deinitialize() override;

	void Activate();
	void RefreshEquippedSlots() const;

	// Event Handlers --------------------------------------------------------------------------------------------------
	void HandleInfoUiOpened();

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
		const USkinDefinition* SkinDefinition);

	UFUNCTION()
	void HandleSkinDroppedToCharacter(const USkinDefinition* SkinDefinition);

	UFUNCTION()
	void HandleSkinFilterTypeClicked(FGameplayTag TypeTag);

	UFUNCTION()
	void HandleSkinFilterAllClicked();

private:
	UFUNCTION()
	void HandleSkinsChanged();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void EquipSkinDefinition(const USkinDefinition* SkinDefinition);

	void BindEvents();
	void UnbindEvents();
	void BindTileItemClicked();
	void ClearTileItemClicked() const;
	void ClearSkinEquipSlot(USkinEquipSlotWidget* TargetSkinEquipSlot, FGameplayTag EquipTypeTag);
	void PopulateAllSkins() const;

private:
	UPROPERTY(Transient)
	TObjectPtr<USkinEquipSlotWidget> SelectedEquipSlot;

	UPROPERTY(Transient)
	FGameplayTag SelectedEquipTypeTag;

	FGameplayTag CurrentFilterTag;
	TWeakObjectPtr<USkinComponent> BoundSkinComponent;
};
