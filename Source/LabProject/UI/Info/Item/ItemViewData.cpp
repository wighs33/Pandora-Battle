#include "UI/Info/Item/ItemViewData.h"

#include "Definition/Item/ItemDefinition.h"
#include "Item/ItemInstance.h"
#include "Definition/Skin/SkinDefinition.h"
#include "Localization/MenuLocalizationSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ItemViewData)

FItemViewData FItemViewDataBuilder::FromItemInstance(const UItemInstance* ItemInstance,
	const UMenuLocalizationSubsystem* Localization, const bool bOwned, const bool bActive)
{
	FItemViewData ViewData;
	ViewData.bOwned = bOwned && ItemInstance != nullptr;
	ViewData.bActive = bActive && ItemInstance != nullptr;
	ViewData.bEnabled = ItemInstance != nullptr;

	const UItemDefinition* ItemDefinition = IsValid(ItemInstance) ? ItemInstance->ItemDefinition.Get() : nullptr;
	if (!ItemDefinition)
	{
		return ViewData;
	}

	ViewData.DisplayName = Localization ? Localization->GetProductText(ItemDefinition, TEXT("Name"), ItemDefinition->DisplayName) : ItemDefinition->DisplayName;
	ViewData.Description = Localization ? Localization->GetProductText(ItemDefinition, TEXT("Description"), ItemDefinition->Description) : ItemDefinition->Description;
	ViewData.IconResource = ItemDefinition->IconTexture.Get();
	ViewData.Stats = BuildItemStatMap(ItemInstance);
	ItemInstance->BuildUpgradeBonusStatMagnitudes(ViewData.UpgradeBonusStats);
	ViewData.Quantity = ItemInstance->Quantity;
	ViewData.UpgradeLevel = ItemInstance->GetUpgradeLevel();
	return ViewData;
}

FItemViewData FItemViewDataBuilder::FromSkinDefinition(const USkinDefinition* SkinDefinition,
	const UMenuLocalizationSubsystem* Localization, const bool bOwned, const bool bActive)
{
	FItemViewData ViewData;
	ViewData.bOwned = bOwned && SkinDefinition != nullptr;
	ViewData.bActive = bActive && SkinDefinition != nullptr;
	ViewData.bEnabled = SkinDefinition != nullptr;

	if (!SkinDefinition)
	{
		return ViewData;
	}

	ViewData.DisplayName = Localization ? Localization->GetProductText(SkinDefinition, TEXT("Name"), SkinDefinition->DisplayName) : SkinDefinition->DisplayName;
	ViewData.Description = Localization ? Localization->GetProductText(SkinDefinition, TEXT("Description"), SkinDefinition->Description) : SkinDefinition->Description;
	ViewData.IconResource = SkinDefinition->IconTexture;
	return ViewData;
}

TMap<FGameplayTag, float> FItemViewDataBuilder::BuildItemStatMap(const UItemInstance* ItemInstance)
{
	TMap<FGameplayTag, float> Result;
	if (IsValid(ItemInstance))
	{
		ItemInstance->BuildEffectiveStatMagnitudes(Result);
	}

	return Result;
}
