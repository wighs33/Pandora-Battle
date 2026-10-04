#include "UI/Shop/ShopWidget.h"
#include "CommonActivatableWidget.h"
#include "Input/CommonUIActionRouterBase.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/TileView.h"
#include "Data/ContentDataSubsystem.h"
#include "Data/ContentLease.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/PlayerController.h"
#include "Definition/Item/ItemDefinition.h"
#include "Engine/GameInstance.h"
#include "Audio/BgmSubsystem.h"
#include "Profile/PlayerProfileSubsystem.h"
#include "Mode/PdPlayerController.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "Definition/Skin/SkinDefinition.h"
#include "Definition/UI/ShopCatalogDefinition.h"
#include "UI/Shop/ShopCatalog.h"
#include "UI/Shop/ShopEntryViewData.h"
#include "UI/Shop/ShopPreviewPanelWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ShopWidget)

namespace
{
	enum class EShopPurchaseResult : uint8
	{
		Purchased,
		NotEnoughGold,
		AlreadyOwned,
		Unsupported
	};

	/** 상품 종류에 맞는 골드 구매를 프로필에 요청한다. 이미 가진 상품은 사지 않는다. 저장은 호출한 쪽이 한다. */
	EShopPurchaseResult PurchaseProductWithGold(UPlayerProfileSubsystem& Profile, UObject* ProductObject,
		const EShopProductType ProductType, const int32 GoldPrice, const int32 PandoraStartingLevel)
	{
		int32 RemainingGold = 0;
		switch (ProductType)
		{
		case EShopProductType::Pandora:
			if (const UPandoraDefinition* PandoraDefinition = Cast<UPandoraDefinition>(ProductObject))
			{
				if (Profile.IsPandoraGranted(PandoraDefinition))
				{
					return EShopPurchaseResult::AlreadyOwned;
				}
				return Profile.TryPurchasePandoraWithGold(PandoraDefinition, GoldPrice, PandoraStartingLevel, RemainingGold, false)
					? EShopPurchaseResult::Purchased
					: EShopPurchaseResult::NotEnoughGold;
			}
			return EShopPurchaseResult::Unsupported;
		case EShopProductType::Skin:
			if (USkinDefinition* SkinDefinition = Cast<USkinDefinition>(ProductObject))
			{
				if (Profile.IsSkinGranted(SkinDefinition))
				{
					return EShopPurchaseResult::AlreadyOwned;
				}
				return Profile.TryPurchaseSkinWithGold(SkinDefinition, GoldPrice, RemainingGold, false)
					? EShopPurchaseResult::Purchased
					: EShopPurchaseResult::NotEnoughGold;
			}
			return EShopPurchaseResult::Unsupported;
		default:
			return EShopPurchaseResult::Unsupported;
		}
	}
}

void UShopWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);

	BindWidgets();
	EnsurePlayerSaveLoaded();
	if (UBgmSubsystem* BgmSubsystem = UGameInstance::GetSubsystem<UBgmSubsystem>(GetGameInstance()))
	{
		BgmSubsystem->PlayBgmForContext(EBgmContext::Shop);
	}
	BeginContentPreload();
	RefreshUI();
}

void UShopWidget::NativeDestruct()
{
	ReleaseContentPreloads();
	UnbindWidgets();
	if (UBgmSubsystem* BgmSubsystem = UGameInstance::GetSubsystem<UBgmSubsystem>(GetGameInstance()))
	{
		BgmSubsystem->RestoreWorldBgm();
	}
	Super::NativeDestruct();
}

void UShopWidget::BeginContentPreload()
{
	ReleaseContentPreloads();
	const int32 PreloadGeneration = ++ContentPreloadGeneration;

	const UGameInstance* GameInstance = GetGameInstance();
	UContentDataSubsystem* ContentDataSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
	if (!ContentDataSubsystem)
	{
		return;
	}

	const FSimpleDelegate RefreshAfterLoad =
		FSimpleDelegate::CreateWeakLambda(
			this,
			[this, PreloadGeneration]()
			{
				if (PreloadGeneration == ContentPreloadGeneration)
				{
					RefreshUI();
				}
			});
	PandoraContentPreloadHandle =
		ContentDataSubsystem->PreloadPandoraDataAssetsAsync(RefreshAfterLoad);
	SkinContentPreloadHandle =
		ContentDataSubsystem->PreloadSkinDataAssetsAsync(RefreshAfterLoad);

	TArray<FSoftObjectPath> CatalogProductPaths;
	const TArray<FShopCatalogProductReference>& ProductReferences =
		ShopCatalogDefinition ? ShopCatalogDefinition->ProductList : ShopProductList;
	for (const FShopCatalogProductReference& ProductReference : ProductReferences)
	{
		switch (ProductReference.ProductType)
		{
		case EShopProductType::Pandora:
			CatalogProductPaths.Add(ProductReference.PandoraDefinition.ToSoftObjectPath());
			break;
		case EShopProductType::Skin:
			CatalogProductPaths.Add(ProductReference.SkinDefinition.ToSoftObjectPath());
			break;
		case EShopProductType::Item:
			CatalogProductPaths.Add(ProductReference.ItemDefinition.ToSoftObjectPath());
			break;
		default:
			break;
		}
	}

	CatalogProductLease = ContentDataSubsystem->AcquireContent(
		CatalogProductPaths,
		FSimpleDelegate::CreateUObject(this, &ThisClass::BeginCatalogPresentationPreload));
}

void UShopWidget::BeginCatalogPresentationPreload()
{
	const UGameInstance* GameInstance = GetGameInstance();
	UContentDataSubsystem* ContentDataSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
	if (!ContentDataSubsystem)
	{
		return;
	}

	TArray<FSoftObjectPath> PresentationPaths;
	const TArray<FShopCatalogProductReference>& ProductReferences =
		ShopCatalogDefinition ? ShopCatalogDefinition->ProductList : ShopProductList;
	for (const FShopCatalogProductReference& ProductReference : ProductReferences)
	{
		if (const UItemDefinition* ItemDefinition =
			Cast<UItemDefinition>(PdShopCatalog::ResolveProductObject(ProductReference)))
		{
			PresentationPaths.Add(ItemDefinition->IconTexture.ToSoftObjectPath());
		}
	}

	CatalogPresentationLease = ContentDataSubsystem->AcquireContent(
		PresentationPaths,
		FSimpleDelegate::CreateUObject(this, &ThisClass::RefreshUI));
}

void UShopWidget::ReleaseContentPreloads()
{
	++ContentPreloadGeneration;
	CatalogPresentationLease.Reset();
	CatalogProductLease.Reset();

	auto ReleaseHandle = [](TSharedPtr<FStreamableHandle>& Handle)
	{
		if (Handle.IsValid())
		{
			Handle->CancelHandle();
			Handle->ReleaseHandle();
			Handle.Reset();
		}
	};

	ReleaseHandle(SkinContentPreloadHandle);
	ReleaseHandle(PandoraContentPreloadHandle);
}

void UShopWidget::RefreshUI()
{
	EnsurePlayerSaveLoaded();
	RebuildEntryData();
	RefreshCategoryButtonStates();

	if (Txt_Gold)
	{
		Txt_Gold->SetText(FText::Format(GoldTextFormat, FText::AsNumber(GetCurrentGold())));
	}

	if (TileView)
	{
		TileView->ClearListItems();
		for (UShopEntryViewData* EntryData : EntryDataList)
		{
			TileView->AddItem(EntryData);
		}
	}

	UShopEntryViewData* NewSelection = SelectedEntryData
		? FindEntryDataByProduct(SelectedEntryData->GetProductObject(), SelectedEntryData->GetProductType())
		: nullptr;
	SelectEntry(NewSelection);
}

void UShopWidget::SelectEntry(UShopEntryViewData* EntryData)
{
	SelectedEntryData = EntryData;

	if (TileView)
	{
		if (EntryData)
		{
			TileView->SetSelectedItem(EntryData);
		}
		else
		{
			TileView->ClearSelection();
		}
	}

	if (WBP_ShopPreviewPanel)
	{
		WBP_ShopPreviewPanel->SetEntryData(EntryData);
		WBP_ShopPreviewPanel->SetVisibility(ESlateVisibility::Visible);
		if (IsPandoraComingSoonEntry(EntryData))
		{
			WBP_ShopPreviewPanel->SetLocalizedMessage(TEXT("Shop.ComingSoon"), PandoraComingSoonText);
		}
	}
}

bool UShopWidget::TryPurchaseSelectedEntry()
{
	if (!SelectedEntryData || !SelectedEntryData->GetProductObject())
	{
		SetMessage(TEXT("Shop.SelectItem"), SelectItemText);
		return false;
	}

	UObject* ProductObject = SelectedEntryData->GetProductObject();
	const EShopProductType ProductType = SelectedEntryData->GetProductType();

	const int32 GoldPrice = SelectedEntryData->GetGoldPrice();

	if (IsPandoraComingSoonProduct(ProductObject, ProductType))
	{
		SetMessage(TEXT("Shop.ComingSoon"), PandoraComingSoonText);
		RefreshAndSelectProduct(ProductObject, ProductType);
		return false;
	}

	if (!SelectedEntryData->CanSell())
	{
		SetMessage(TEXT("Shop.NotForSale"), NotForSaleText);
		return false;
	}

	if (!CanPurchaseProductType(ProductType))
	{
		SetMessage(TEXT("Shop.Unsupported"), UnsupportedProductTypeText);
		return false;
	}

	UPlayerProfileSubsystem* ProfileSubsystem = UGameInstance::GetSubsystem<UPlayerProfileSubsystem>(GetGameInstance());
	if (!ProfileSubsystem)
	{
		return false;
	}

	switch (PurchaseProductWithGold(*ProfileSubsystem, ProductObject, ProductType, GoldPrice, PurchasedPandoraStartingLevel))
	{
	case EShopPurchaseResult::Unsupported:
		SetMessage(TEXT("Shop.Unsupported"), UnsupportedProductTypeText);
		return false;
	case EShopPurchaseResult::AlreadyOwned:
		SetMessage(TEXT("Shop.AlreadyOwned"), AlreadyOwnedText);
		RefreshAndSelectProduct(ProductObject, ProductType);
		return false;
	case EShopPurchaseResult::NotEnoughGold:
		SetMessage(TEXT("Shop.NeedGold"), NotEnoughGoldText);
		RefreshAndSelectProduct(ProductObject, ProductType);
		return false;
	case EShopPurchaseResult::Purchased:
		break;
	}

	SetMessage(TEXT("Shop.Purchased"), PurchaseSucceededTextFormat, ProductObject);
	ProfileSubsystem->SaveProfile();
	if (APdPlayerController* PdPlayerController = Cast<APdPlayerController>(GetOwningPlayer()))
	{
		PdPlayerController->RequestLocalCosmeticProfileSync();
	}
	RefreshAndSelectProduct(ProductObject, ProductType);
	return true;
}

void UShopWidget::RefreshAndSelectProduct(UObject* ProductObject, const EShopProductType ProductType)
{
	RefreshUI();
	SelectEntry(FindEntryDataByProduct(ProductObject, ProductType));
}

void UShopWidget::HandleCloseClicked()
{
	if (UCommonActivatableWidget* Screen = UCommonUIActionRouterBase::FindOwningActivatable(GetCachedWidget(), GetOwningLocalPlayer()))
		Screen->DeactivateWidget();
	RemoveFromParent();
}

void UShopWidget::HandlePandoraCategoryClicked()
{
	SetActiveCategory(EShopProductType::Pandora);
}

void UShopWidget::HandleSkinCategoryClicked()
{
	SetActiveCategory(EShopProductType::Skin);
}

void UShopWidget::HandleBuyRequested(UShopEntryViewData* EntryData)
{
	SelectEntry(EntryData);
	TryPurchaseSelectedEntry();
}

void UShopWidget::HandleResetShopSaveClicked()
{
	UPlayerProfileSubsystem* ProfileSubsystem = UGameInstance::GetSubsystem<UPlayerProfileSubsystem>(GetGameInstance());
	if (!ProfileSubsystem)
	{
		return;
	}

	UObject* SelectedProductObject = SelectedEntryData ? SelectedEntryData->GetProductObject() : nullptr;
	const EShopProductType SelectedProductType = SelectedEntryData ? SelectedEntryData->GetProductType() : ActiveCategory;

	const bool bReset = ProfileSubsystem->ResetPurchasedProgress(false);
	if (bReset)
	{
		ProfileSubsystem->SaveProfile();
	}

	RefreshUI();
	if (SelectedProductObject)
	{
		SelectEntry(FindEntryDataByProduct(SelectedProductObject, SelectedProductType));
	}

	if (bReset)
	{
		SetMessage(TEXT("Shop.ResetDone"), ShopSaveResetText);
	}
}

void UShopWidget::BindWidgets()
{
	if (bWidgetsBound)
	{
		return;
	}

	if (Btn_Close)
	{
		Btn_Close->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCloseClicked);
	}

	if (PandoraCategoryButton)
	{
		PandoraCategoryButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandlePandoraCategoryClicked);
	}

	if (SkinCategoryButton)
	{
		SkinCategoryButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSkinCategoryClicked);
	}

	if (Btn_ResetShopSave)
	{
		Btn_ResetShopSave->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleResetShopSaveClicked);
	}

	if (TileView)
	{
		TileView->OnItemClicked().AddUObject(this, &ThisClass::HandleTileViewItemClicked);
	}

	if (WBP_ShopPreviewPanel)
	{
		WBP_ShopPreviewPanel->OnBuyRequested.AddUniqueDynamic(this, &ThisClass::HandleBuyRequested);
	}

	bWidgetsBound = true;
}

void UShopWidget::UnbindWidgets()
{
	if (!bWidgetsBound)
	{
		return;
	}

	if (Btn_Close)
	{
		Btn_Close->OnClicked.RemoveDynamic(this, &ThisClass::HandleCloseClicked);
	}

	if (PandoraCategoryButton)
	{
		PandoraCategoryButton->OnClicked.RemoveDynamic(this, &ThisClass::HandlePandoraCategoryClicked);
	}

	if (SkinCategoryButton)
	{
		SkinCategoryButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleSkinCategoryClicked);
	}

	if (Btn_ResetShopSave)
	{
		Btn_ResetShopSave->OnClicked.RemoveDynamic(this, &ThisClass::HandleResetShopSaveClicked);
	}

	if (TileView)
	{
		TileView->OnItemClicked().RemoveAll(this);
	}

	if (WBP_ShopPreviewPanel)
	{
		WBP_ShopPreviewPanel->OnBuyRequested.RemoveDynamic(this, &ThisClass::HandleBuyRequested);
	}

	bWidgetsBound = false;
}

void UShopWidget::SetActiveCategory(const EShopProductType NewCategory)
{
	if (ActiveCategory == NewCategory)
	{
		RefreshUI();
		return;
	}

	ActiveCategory = NewCategory;
	SelectedEntryData = nullptr;
	if (TileView)
	{
		TileView->ClearSelection();
	}
	RefreshUI();
}

void UShopWidget::RefreshCategoryButtonStates() const
{
	if (PandoraCategoryButton)
	{
		PandoraCategoryButton->SetIsEnabled(ActiveCategory != EShopProductType::Pandora);
	}

	if (SkinCategoryButton)
	{
		SkinCategoryButton->SetIsEnabled(ActiveCategory != EShopProductType::Skin);
	}
}

void UShopWidget::RebuildEntryData()
{
	EntryDataList.Reset();

	const int32 CurrentGold = GetCurrentGold();
	for (const FShopCatalogEntry& CatalogEntry : BuildEffectiveCatalog())
	{
		if (CatalogEntry.Product.ProductType != ActiveCategory)
		{
			continue;
		}

		if (!CatalogEntry.HasValidProduct())
		{
			continue;
		}

		UObject* ProductObject = PdShopCatalog::ResolveProductObject(CatalogEntry.Product);
		if (!ProductObject)
		{
			continue;
		}

		FShopCatalogEntry EffectiveCatalogEntry = CatalogEntry;
		if (IsPandoraComingSoonProduct(ProductObject, EffectiveCatalogEntry.Product.ProductType))
		{
			EffectiveCatalogEntry.ShopData.bCanSell = false;
		}

		UShopEntryViewData* EntryData = NewObject<UShopEntryViewData>(this);
		EntryData->Initialize(EffectiveCatalogEntry, ProductObject, CurrentGold, IsProductOwned(ProductObject, CatalogEntry.Product.ProductType));
		EntryData->OnClicked.AddUObject(this, &ThisClass::SelectEntry);
		EntryDataList.Add(EntryData);
	}
}

TArray<FShopCatalogEntry> UShopWidget::BuildEffectiveCatalog() const
{
	FShopCatalogSource Source;
	Source.ManualProducts = ShopCatalogDefinition ? ShopCatalogDefinition->ProductList : ShopProductList;
	Source.bIncludeAllPandoras = ShopCatalogDefinition ? ShopCatalogDefinition->bAutoIncludeAllPandoras : bAutoIncludeAllPandoras;
	Source.bIncludeAllSkins = ShopCatalogDefinition ? ShopCatalogDefinition->bAutoIncludeAllSkins : bAutoIncludeAllSkins;

	const UGameInstance* GameInstance = GetGameInstance();
	return PdShopCatalog::Build(Source, GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr);
}

int32 UShopWidget::GetCurrentGold() const
{
	UPlayerProfileSubsystem* ProfileSubsystem = UGameInstance::GetSubsystem<UPlayerProfileSubsystem>(GetGameInstance());
	return ProfileSubsystem ? ProfileSubsystem->GetGold() : 0;
}

bool UShopWidget::IsProductOwned(UShopEntryViewData* EntryData) const
{
	return EntryData && IsProductOwned(EntryData->GetProductObject(), EntryData->GetProductType());
}

bool UShopWidget::IsProductOwned(UObject* ProductObject, const EShopProductType ProductType) const
{
	UPlayerProfileSubsystem* ProfileSubsystem = UGameInstance::GetSubsystem<UPlayerProfileSubsystem>(GetGameInstance());
	if (!ProfileSubsystem || !ProductObject)
	{
		return false;
	}

	switch (ProductType)
	{
	case EShopProductType::Pandora:
		return ProfileSubsystem->IsPandoraGranted(Cast<UPandoraDefinition>(ProductObject));
	case EShopProductType::Skin:
		return ProfileSubsystem->IsSkinGranted(Cast<USkinDefinition>(ProductObject));
	case EShopProductType::Item:
	default:
		return false;
	}
}

bool UShopWidget::IsPandoraAllowedForShopPurchase(const UObject* ProductObject) const
{
	const UPandoraDefinition* PandoraDefinition = Cast<UPandoraDefinition>(ProductObject);
	if (!PandoraDefinition)
	{
		return true;
	}

	FString PandoraKey = PandoraDefinition->GetFName().ToString();
	PandoraKey.RemoveFromStart(TEXT("DA_Pandora_"), ESearchCase::IgnoreCase);
	PandoraKey.RemoveFromStart(TEXT("Pandora_"), ESearchCase::IgnoreCase);

	for (const FName AllowedKeyName : PurchasablePandoraKeys)
	{
		FString AllowedKey = AllowedKeyName.ToString();
		AllowedKey.RemoveFromStart(TEXT("DA_Pandora_"), ESearchCase::IgnoreCase);
		AllowedKey.RemoveFromStart(TEXT("Pandora_"), ESearchCase::IgnoreCase);
		if (PandoraKey.Equals(AllowedKey, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	return false;
}

bool UShopWidget::IsPandoraComingSoonProduct(const UObject* ProductObject, const EShopProductType ProductType) const
{
	return ProductType == EShopProductType::Pandora && !IsPandoraAllowedForShopPurchase(ProductObject);
}

bool UShopWidget::IsPandoraComingSoonEntry(const UShopEntryViewData* EntryData) const
{
	return EntryData && IsPandoraComingSoonProduct(EntryData->GetProductObject(), EntryData->GetProductType());
}

bool UShopWidget::CanPurchaseProductType(const EShopProductType ProductType) const
{
	switch (ProductType)
	{
	case EShopProductType::Pandora:
	case EShopProductType::Skin:
		return true;
	case EShopProductType::Item:
	default:
		return false;
	}
}

void UShopWidget::EnsurePlayerSaveLoaded() const
{
	UPlayerProfileSubsystem* ProfileSubsystem = UGameInstance::GetSubsystem<UPlayerProfileSubsystem>(GetGameInstance());
	if (!ProfileSubsystem)
	{
		return;
	}

	ProfileSubsystem->LoadProfile();
}

void UShopWidget::SetMessage(FName Key, const FText& Fallback, UObject* Product) const
{
	if (WBP_ShopPreviewPanel)
	{
		WBP_ShopPreviewPanel->SetLocalizedMessage(Key, Fallback, Product);
	}
}

UShopEntryViewData* UShopWidget::FindEntryDataByProduct(UObject* ProductObject, const EShopProductType ProductType) const
{
	if (!ProductObject)
	{
		return nullptr;
	}

	for (UShopEntryViewData* EntryData : EntryDataList)
	{
		if (EntryData && EntryData->GetProductObject() == ProductObject && EntryData->GetProductType() == ProductType)
		{
			return EntryData;
		}
	}

	return nullptr;
}

void UShopWidget::HandleTileViewItemClicked(UObject* ItemObject)
{
	SelectEntry(Cast<UShopEntryViewData>(ItemObject));
}

