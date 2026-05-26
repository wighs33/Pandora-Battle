#pragma once

#include "CoreMinimal.h"
#include "Common/Enum_Direction.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "InfoUiPresenter.generated.h"

class APdPlayerController;
class UInventoryComponent;
class UEquipSlotWidget;
class UInfoWidget;
class UItemDefinition;
class UItemInstance;
class UPandoraDefinition;
class UPandoraInstance;
class UPandoraEquipSlotWidget;
class USkinEquipSlotWidget;

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API UInfoUiPresenter : public UObject
{
	GENERATED_BODY()

public:
	UInfoUiPresenter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual UWorld* GetWorld() const override;

	void Initialize(APdPlayerController* InController);
	void Deinitialize();
	void BindInfoUi(UInfoWidget* InInfoWidget);
	UItemInstance* GetSelectedWeapon(EEnum_Direction Direction) const;
	UPandoraInstance* GetSelectedPandora(EEnum_Direction Direction) const;

	UFUNCTION()
	void HandleSelectedPandoraDirection(EEnum_Direction Direction);

	UFUNCTION()
	void HandleOpenedInfoUi();

	UFUNCTION()
	void HandleClickedStatUpButton(FGameplayTag StatTag);

	UFUNCTION()
	void HandleClickedInfoCenterButton(FGameplayTag LeftUiTag, FGameplayTag RightUiTag);

	UFUNCTION()
	void HandleClickedItemSlot(UObject* Item);

	UFUNCTION()
	void HandleClickedItemEquipTypeSlot(FGameplayTag EquipTypeTag, UEquipSlotWidget* SelectedEquipSlot, bool bIsSelectedAnyButton);

	UFUNCTION()
	void HandleClickedItemFilterTypeButton(FGameplayTag TypeTag);

	UFUNCTION()
	void HandleClickedItemFilterAllButton();

	UFUNCTION()
	void HandleClickedSkinSlot(UObject* Item);

	UFUNCTION()
	void HandleClickedSkinEquipTypeSlot(FGameplayTag EquipTypeTag, USkinEquipSlotWidget* SelectedEquipSlot, bool bIsSelectedAnyButton);

	UFUNCTION()
	void HandleClickedSkinFilterTypeButton(FGameplayTag TypeTag);

	UFUNCTION()
	void HandleClickedSkinFilterAllButton();

	UFUNCTION()
	void HandleClickedPandoraSlot(UObject* Item);

	UFUNCTION()
	void HandleClickedPandoraEquipSlot(UPandoraEquipSlotWidget* SelectedPandoraEquipSlot, bool bIsSelectedAnyButton);

	UFUNCTION()
	void HandleClickedPandoraFilterTypeButton(FGameplayTag TypeTag);

	UFUNCTION()
	void HandleClickedPandoraFilterAllButton();

private:
	UInfoWidget* GetInfoWidget() const;
	class USelectPandoraWidget* GetSelectPandoraWidget() const;
	APdPlayerController* GetController() const;
	class APdPlayerState* GetCachedPlayerState() const;
	void BindInventoryChangeNotification();
	void UnbindInventoryChangeNotification();
	void BindStatusWidgetEvents();
	void HandleInventoryChanged();
	void RefreshInventoryTileView();
	FGameplayTag GetProfileLeftUiTag() const;
	FGameplayTag GetEquipmentLeftUiTag() const;
	FGameplayTag GetSkinEquipmentLeftUiTag() const;
	FGameplayTag GetPandoraEquipmentLeftUiTag() const;
	FGameplayTag GetWeaponItemTypeTag() const;
	void RefreshSelectPandoraCompatibilityState() const;
	void RefreshSelectPandoraLoadoutImages() const;

	UPROPERTY(Transient)
	TObjectPtr<APdPlayerController> OwningController = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UInfoWidget> InfoWidget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UInventoryComponent> BoundInventoryComponent = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UEquipSlotWidget> CachedSelectedEquipSlot = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USkinEquipSlotWidget> CachedSelectedSkinEquipSlot = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UPandoraEquipSlotWidget> CachedSelectedPandoraEquipSlot = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UItemInstance> CachedFirstWeapon = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UItemInstance> CachedSecondWeapon = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UItemInstance> CachedThirdWeapon = nullptr;

	UPROPERTY(Transient)
	FGameplayTag CurrentLeftUiTag;

	UPROPERTY(Transient)
	FGameplayTag CurrentItemFilterTag;

	UPROPERTY(Transient)
	bool bUseItemTypeFilter = false;
};
