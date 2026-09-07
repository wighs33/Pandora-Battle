#pragma once

#include "CoreMinimal.h"
#include "Common/Enum_Direction.h"
#include "GameplayTagContainer.h"
#include "UI/Presenter/InfoTabPresenterBase.h"
#include "InfoPandoraTabPresenter.generated.h"

class UInfoLoadoutStore;
class UItemInstance;
class UPandoraComponent;
class UPandoraDefinition;
class UPandoraEquipSlotWidget;
class UPandoraInstance;
class UTileView;

UCLASS()
class LABPROJECT_API UInfoPandoraTabPresenter : public UInfoTabPresenterBase
{
	GENERATED_BODY()

public:
	virtual void BindInfoUi(UInfoWidget* InInfoWidget) override;
	virtual void Deinitialize() override;

	void SetLoadoutStore(UInfoLoadoutStore* InLoadoutStore);
	void SetActive(bool bInActive);
	void Activate();
	void HandleInfoUiOpened();
	void HandleInventoryChanged();
	void HandleWeaponLoadoutChanged();
	void HandlePandoraLoadoutChanged();
	void HandlePresentationAssetsReady();

	void RefreshLoadoutPresentation() const;

	UFUNCTION()
	void HandlePandoraSlotClicked(UObject* Item);

	UFUNCTION()
	void HandlePandoraEquipSlotClicked(
		UPandoraEquipSlotWidget* SelectedPandoraEquipSlot,
		bool bIsSelectedAnyButton);

	UFUNCTION()
	void HandlePandoraFilterTypeClicked(FGameplayTag TypeTag);

	UFUNCTION()
	void HandlePandoraFilterAllClicked();

private:
	UFUNCTION()
	void HandlePandoraInventoryChanged();

	void BindEvents();
	void UnbindEvents();
	class USelectPandoraWidget* GetSelectPandoraWidget() const;
	UItemInstance* GetSelectedWeapon(EEnum_Direction Direction) const;
	void RefreshLeftPandoraSlots() const;
	void RefreshSelectPandoraImages() const;
	void RefreshSelectPandoraCompatibility() const;
	bool IsPandoraOwned(const UPandoraInstance* PandoraInstance) const;
	void BuildPandoraTileViewItems(
		TArray<UObject*>& OutListItems,
		FGameplayTag TypeTag,
		bool bOwnedOnly) const;
	void RefreshPandoraTileView() const;
	void BindPandoraTileItemClicked();
	void UnbindPandoraTileItemClicked();
	bool ResolveLoadoutSlotForClick(
		const UPandoraComponent* PandoraComponent,
		const UPandoraDefinition* PandoraDefinition,
		EEnum_Direction& OutDirection,
		int32& OutSlotNumber) const;
	void ResetEquipSlotClickState();
	void ClearPandoraEquipSlot(UPandoraEquipSlotWidget* TargetPandoraEquipSlot);

	UPROPERTY(Transient)
	TObjectPtr<UPandoraEquipSlotWidget> SelectedEquipSlot;

	UPROPERTY(Transient)
	FGameplayTag CurrentFilterTag;

	TWeakObjectPtr<UInfoLoadoutStore> LoadoutStore;
	TWeakObjectPtr<UPandoraComponent> BoundPandoraComponent;
	TWeakObjectPtr<UTileView> BoundTileView;
	FDelegateHandle TileItemClickedDelegateHandle;
	bool bUseTypeFilter = false;
	bool bShowOnlyOwnedForEquipSlot = false;
	bool bActive = false;
};
