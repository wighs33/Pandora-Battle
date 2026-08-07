#pragma once

#include "CoreMinimal.h"
#include "UI/Widget/ItemViewData.h"
#include "UObject/Object.h"
#include "InventorySlotViewData.generated.h"

class UItemInstance;

UCLASS(BlueprintType)
class LABPROJECT_API UInventorySlotViewData : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(
		int32 InSlotIndex,
		UItemInstance* InItemInstance,
		bool bInDuplicateWeaponOrEquipment = false);

	UFUNCTION(BlueprintPure, Category = "!UI|Inventory")
	int32 GetSlotIndex() const { return SlotIndex; }

	UFUNCTION(BlueprintPure, Category = "!UI|Inventory")
	UItemInstance* GetItemInstance() const { return ItemInstance; }

	UFUNCTION(BlueprintPure, Category = "!UI|Inventory")
	bool IsEmpty() const { return ItemInstance == nullptr; }

	UFUNCTION(BlueprintPure, Category = "!UI|Inventory")
	FItemViewData GetViewData() const { return ViewData; }

	UFUNCTION(BlueprintPure, Category = "!UI|Inventory")
	bool IsDuplicateWeaponOrEquipment() const { return bDuplicateWeaponOrEquipment; }

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!UI|Inventory", meta = (AllowPrivateAccess = "true"))
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!UI|Inventory", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UItemInstance> ItemInstance;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!UI|Inventory", meta = (AllowPrivateAccess = "true"))
	FItemViewData ViewData;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!UI|Inventory", meta = (AllowPrivateAccess = "true"))
	bool bDuplicateWeaponOrEquipment = false;
};
