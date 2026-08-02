#include "UI/Widget/ItemViewData.h"

#include "Definition/Item/ItemDefinition.h"
#include "Item/ItemInstance.h"
#include "Definition/Skin/SkinDefinition.h"
#include "Skin/SkinInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ItemViewData)

FPdItemViewData FItemViewDataBuilder::FromItemInstance(
	const UItemInstance* ItemInstance,
	const bool bOwned,
	const bool bActive)
{
	FPdItemViewData ViewData;
	ViewData.bOwned = bOwned && ItemInstance != nullptr;
	ViewData.bActive = bActive && ItemInstance != nullptr;
	ViewData.bEnabled = ItemInstance != nullptr;

	const UItemDefinition* ItemDefinition = IsValid(ItemInstance) ? ItemInstance->ItemDefinition.Get() : nullptr;
	if (!ItemDefinition)
	{
		return ViewData;
	}

	ViewData.DisplayName = ItemDefinition->DisplayName;
	ViewData.Description = ItemDefinition->Description;
	ViewData.IconResource = ItemDefinition->IconTexture.Get();
	ViewData.Stats = BuildItemStatMap(ItemInstance);
	ViewData.Quantity = ItemInstance->Quantity;
	return ViewData;
}

FPdItemViewData FItemViewDataBuilder::FromSkinInstance(
	const USkinInstance* SkinInstance,
	const bool bOwned,
	const bool bActive)
{
	const USkinDefinition* SkinDefinition = IsValid(SkinInstance) ? SkinInstance->SkinDefinition.Get() : nullptr;
	return FromSkinDefinition(SkinDefinition, bOwned && SkinInstance != nullptr, bActive && SkinInstance != nullptr);
}

FPdItemViewData FItemViewDataBuilder::FromSkinDefinition(
	const USkinDefinition* SkinDefinition,
	const bool bOwned,
	const bool bActive)
{
	FPdItemViewData ViewData;
	ViewData.bOwned = bOwned && SkinDefinition != nullptr;
	ViewData.bActive = bActive && SkinDefinition != nullptr;
	ViewData.bEnabled = SkinDefinition != nullptr;

	if (!SkinDefinition)
	{
		return ViewData;
	}

	ViewData.DisplayName = SkinDefinition->DisplayName;
	ViewData.Description = SkinDefinition->Description;
	ViewData.IconResource = SkinDefinition->IconTexture;
	return ViewData;
}

TMap<FGameplayTag, float> FItemViewDataBuilder::BuildItemStatMap(const UItemInstance* ItemInstance)
{
	TMap<FGameplayTag, float> Result;

	const UItemDefinition* ItemDefinition = IsValid(ItemInstance) ? ItemInstance->ItemDefinition.Get() : nullptr;
	if (ItemDefinition)
	{
		Result = ItemDefinition->Map_Stat_Magnitude;
	}

	if (ItemInstance)
	{
		for (const TPair<FGameplayTag, float>& EnhancedStat : ItemInstance->Map_EnhancedStat_Magnitude)
		{
			Result.FindOrAdd(EnhancedStat.Key) += EnhancedStat.Value;
		}
	}

	return Result;
}
