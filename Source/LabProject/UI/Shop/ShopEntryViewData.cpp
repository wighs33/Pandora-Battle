#include "UI/Shop/ShopEntryViewData.h"

#include "Definition/Item/ItemDefinition.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "Definition/Skin/SkinDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ShopEntryViewData)

void UShopEntryViewData::Initialize(
	const FShopCatalogEntry& InCatalogEntry,
	UObject* InProductObject,
	const int32 InPlayerGold,
	const bool bInOwned)
{
	CatalogEntry = InCatalogEntry;
	ProductObject = InProductObject;

	UiData = FShopEntryUiData();
	UiData.bValid = IsValid(InProductObject);
	UiData.GoldPrice = FMath::Max(0, InCatalogEntry.ShopData.GoldPrice);
	UiData.bOwned = bInOwned;
	UiData.bCanAfford = InPlayerGold >= UiData.GoldPrice;
	UiData.bCanSell = InCatalogEntry.ShopData.bCanSell;
	UiData.ProductObject = InProductObject;
	UiData.ProductType = InCatalogEntry.Product.ProductType;

	if (!InProductObject)
	{
		return;
	}

	FText FallbackDisplayName = FText::FromName(InProductObject->GetFName());
	FText FallbackDescription;
	UObject* FallbackIcon = nullptr;

	if (const UPandoraDefinition* PandoraDefinition = Cast<UPandoraDefinition>(InProductObject))
	{
		FallbackDisplayName = PandoraDefinition->GetDisplayName();
		FallbackDescription = PandoraDefinition->GetDescription();
		FallbackIcon = PandoraDefinition->GetIconResource();
	}
	else if (const USkinDefinition* SkinDefinition = Cast<USkinDefinition>(InProductObject))
	{
		FallbackDisplayName = SkinDefinition->DisplayName.IsEmpty() ? FallbackDisplayName : SkinDefinition->DisplayName;
		FallbackDescription = SkinDefinition->Description;
		FallbackIcon = SkinDefinition->IconTexture;
	}
	else if (const UItemDefinition* ItemDefinition = Cast<UItemDefinition>(InProductObject))
	{
		FallbackDisplayName = ItemDefinition->DisplayName.IsEmpty() ? FallbackDisplayName : ItemDefinition->DisplayName;
		FallbackDescription = ItemDefinition->Description;
		FallbackIcon = ItemDefinition->IconTexture.Get();
	}

	UiData.DisplayName = FallbackDisplayName;
	UiData.Description = FallbackDescription;
	UiData.IconResource = FallbackIcon;
}

UPandoraDefinition* UShopEntryViewData::GetPandoraDefinition() const
{
	return Cast<UPandoraDefinition>(ProductObject.Get());
}

void UShopEntryViewData::BroadcastClicked()
{
	OnClicked.Broadcast(this);
}
