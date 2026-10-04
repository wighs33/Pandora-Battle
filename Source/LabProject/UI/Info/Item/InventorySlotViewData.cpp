#include "UI/Info/Item/InventorySlotViewData.h"

#include "Item/ItemInstance.h"
#include "UI/Info/Item/ItemViewData.h"
#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Localization/MenuLocalizationSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InventorySlotViewData)

void UInventorySlotViewData::Initialize(const int32 InSlotIndex, UItemInstance* InItemInstance,
	const bool bInDuplicateWeaponOrEquipment, const bool bInAssigned)
{
	SlotIndex = InSlotIndex;
	ItemInstance = InItemInstance;
	const UUserWidget* OwnerWidget = GetTypedOuter<UUserWidget>();
	const UMenuLocalizationSubsystem* Localization = UGameInstance::GetSubsystem<UMenuLocalizationSubsystem>(OwnerWidget ? OwnerWidget->GetGameInstance() : nullptr);
	ViewData = FItemViewDataBuilder::FromItemInstance(InItemInstance, Localization);
	bDuplicateWeaponOrEquipment = bInDuplicateWeaponOrEquipment;
	bAssigned = bInAssigned;
}
