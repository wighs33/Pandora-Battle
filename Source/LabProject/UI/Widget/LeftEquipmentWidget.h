#pragma once

#include "CoreMinimal.h"
#include "UI/Widget/LocalizedMenuWidget.h"
#include "GameplayTagContainer.h"
#include "UI/Widget/EquipSlotWidget.h"
#include "LeftEquipmentWidget.generated.h"

class UPandoraDefinition;
class UTexture2D;
struct FStreamableHandle;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FPdOnClickedEquipTypeSlot,
	FGameplayTag, EquipTypeTag,
	UEquipSlotWidget*, SelectedEquipSlot,
	bool, bIsSelectedAnyButton);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FPdOnDroppedItemEquipTypeSlot,
	FGameplayTag, EquipTypeTag,
	UEquipSlotWidget*, TargetEquipSlot,
	UItemInstance*, ItemInstance);

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API ULeftEquipmentWidget : public ULocalizedMenuWidget
{
	GENERATED_BODY()

protected:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

public:
	// Public API ------------------------------------------------------------------------------------------------------
	ULeftEquipmentWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "!UI|Equipment")
	void InitialzeEquipSlots();

	UFUNCTION(BlueprintCallable, Category = "!UI|Equipment")
	void ToggleActiveEquipSlots(bool bActive);

	UFUNCTION(BlueprintCallable, Category = "!UI|Equipment")
	void SelectEquipSlot(FGameplayTag EquipTypeTag, UEquipSlotWidget* InSelectedEquipSlot);

	UFUNCTION(BlueprintCallable, Category = "!UI|Equipment")
	void SetWeaponSlotData(int32 WeaponSlotNumber, UItemInstance* ItemInstance);

	UFUNCTION(BlueprintCallable, Category = "!UI|Equipment|Pandora")
	void SetWeaponSlotPandoraRequirement(
		int32 WeaponSlotNumber,
		const UPandoraDefinition* PandoraDefinition);

	UFUNCTION(BlueprintCallable, Category = "!UI|Equipment")
	void SetConsumableQuickSlotData(int32 QuickSlotNumber, UItemInstance* ItemInstance);

	UFUNCTION(BlueprintCallable, Category = "!UI|Equipment")
	void SetEquipmentSlotData(FGameplayTag EquipTypeTag, UItemInstance* ItemInstance);

	UFUNCTION(BlueprintPure, Category = "!UI|Equipment")
	UEquipSlotWidget* FindFirstCompatibleEquipSlot(UItemInstance* ItemInstance) const;

	UFUNCTION(BlueprintPure, Category = "!UI|Equipment")
	UEquipSlotWidget* FindFirstEquippedCompatibleEquipSlot(UItemInstance* ItemInstance) const;

	UFUNCTION(BlueprintCallable, Category = "!UI|Equipment", meta = (Categories = "Item"))
	void BroadcastClickedEquipTypeSlot(FGameplayTag EquipTypeTag, UEquipSlotWidget* InSelectedEquipSlot, bool bInIsSelectedAnyButton);

protected:
	// Event Handlers --------------------------------------------------------------------------------------------------
	virtual void OnMenuLanguageChanged() override;

private:
	UFUNCTION()
	void HandleEquipSlotClicked(UEquipSlotWidget* ItemSlot);

	UFUNCTION()
	void HandleEquipSlotItemDropped(UEquipSlotWidget* ItemSlot, UItemInstance* ItemInstance);

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void RebuildEquipSlotList();
	void RebuildEquipSlotNameList();
	void ApplyEquipSlotNames();
	void ApplyResolvedEquipTypeTags();
	void BindEquipSlotCallbacks();
	void UnbindEquipSlotCallbacks();
	void BeginPandoraWeaponIconPreload();
	void ReleasePandoraWeaponIconPreload();
	void RefreshCachedPandoraWeaponRequirements();
	UEquipSlotWidget* GetWeaponSlot(int32 WeaponSlotNumber) const;
	UTexture2D* ResolvePandoraWeaponRequirementIcon(const UPandoraDefinition* PandoraDefinition) const;
	FGameplayTag ResolveEquipTypeTagForSlot(const UEquipSlotWidget* ItemSlot) const;

public:
	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "!UI|Equipment")
	FPdOnClickedEquipTypeSlot OnClicked_EquipTypeSlot;

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "!UI|Equipment")
	FPdOnDroppedItemEquipTypeSlot OnDroppedItem_EquipTypeSlot;

protected:
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
	TObjectPtr<UEquipSlotWidget> ToolSlot1;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Equipment|Bind")
	TObjectPtr<UEquipSlotWidget> ToolSlot2;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Equipment|Bind")
	TObjectPtr<UEquipSlotWidget> ToolSlot3;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Equipment|Bind")
	TObjectPtr<UEquipSlotWidget> ToolSlot4;

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

	UPROPERTY(EditDefaultsOnly, Category = "!UI|Equipment|Pandora", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UTexture2D> PandoraAxeIcon;

	UPROPERTY(EditDefaultsOnly, Category = "!UI|Equipment|Pandora", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UTexture2D> PandoraBowIcon;

	UPROPERTY(EditDefaultsOnly, Category = "!UI|Equipment|Pandora", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UTexture2D> PandoraDaggerIcon;

	UPROPERTY(EditDefaultsOnly, Category = "!UI|Equipment|Pandora", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UTexture2D> PandoraGreatswordIcon;

	UPROPERTY(EditDefaultsOnly, Category = "!UI|Equipment|Pandora", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UTexture2D> PandoraSwordIcon;

	UPROPERTY(EditDefaultsOnly, Category = "!UI|Equipment|Pandora", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UTexture2D> PandoraGunIcon;

	UPROPERTY(EditDefaultsOnly, Category = "!UI|Equipment|Pandora", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PandoraWeaponRequirementOpacity = 0.3f;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UPandoraDefinition>> CachedWeaponSlotPandoraRequirements;

	int32 PandoraWeaponIconPreloadGeneration = 0;
	TSharedPtr<FStreamableHandle> PandoraWeaponIconPreloadHandle;
};
