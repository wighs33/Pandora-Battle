#include "UI/Shop/ShopWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/TileView.h"
#include "Data/ContentDataSubsystem.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Definition/Item/ItemDefinition.h"
#include "Engine/GameInstance.h"
#include "Settings/BgmSubsystem.h"
#include "SavedGameData/PlayerProfileSubsystem.h"
#include "Mode/PdPlayerController.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "SavedGameData/PdSaveGame.h"
#include "Definition/Skin/SkinDefinition.h"
#include "Definition/UI/ShopCatalogDefinition.h"
#include "UI/Shop/ShopEntryViewData.h"
#include "UI/Shop/ShopPreviewPanelWidget.h"
#include "UI/WidgetLookup.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ShopWidget)

void UShopWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ResolveWidgets();
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

	CatalogProductPreloadHandle =
		ContentDataSubsystem->PreloadSoftObjectPathsAsync(
			CatalogProductPaths,
			FSimpleDelegate::CreateWeakLambda(
				this,
				[this, PreloadGeneration]()
				{
					BeginCatalogPresentationPreload(PreloadGeneration);
				}));
}

void UShopWidget::BeginCatalogPresentationPreload(const int32 PreloadGeneration)
{
	if (PreloadGeneration != ContentPreloadGeneration)
	{
		return;
	}

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
			Cast<UItemDefinition>(ResolveProductObject(ProductReference)))
		{
			PresentationPaths.Add(ItemDefinition->IconTexture.ToSoftObjectPath());
		}
	}

	CatalogPresentationPreloadHandle =
		ContentDataSubsystem->PreloadSoftObjectPathsAsync(
			PresentationPaths,
			FSimpleDelegate::CreateWeakLambda(
				this,
				[this, PreloadGeneration]()
				{
					if (PreloadGeneration == ContentPreloadGeneration)
					{
						RefreshUI();
					}
				}));
}

void UShopWidget::ReleaseContentPreloads()
{
	++ContentPreloadGeneration;

	auto ReleaseHandle = [](TSharedPtr<FStreamableHandle>& Handle)
	{
		if (Handle.IsValid())
		{
			Handle->CancelHandle();
			Handle->ReleaseHandle();
			Handle.Reset();
		}
	};

	ReleaseHandle(CatalogPresentationPreloadHandle);
	ReleaseHandle(CatalogProductPreloadHandle);
	ReleaseHandle(SkinContentPreloadHandle);
	ReleaseHandle(PandoraContentPreloadHandle);
}

void UShopWidget::RefreshUI()
{
	ResolveWidgets();
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
		WBP_ShopPreviewPanel->SetVisibility(EntryData ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (IsPandoraComingSoonEntry(EntryData))
		{
			WBP_ShopPreviewPanel->SetMessage(PandoraComingSoonText);
		}
	}
}

bool UShopWidget::TryPurchaseSelectedEntry()
{
	if (!SelectedEntryData || !SelectedEntryData->GetProductObject())
	{
		SetMessage(SelectItemText);
		return false;
	}

	UObject* ProductObject = SelectedEntryData->GetProductObject();
	const EShopProductType ProductType = SelectedEntryData->GetProductType();
	const FText PurchasedName = SelectedEntryData->GetDisplayName();
	const int32 GoldPrice = SelectedEntryData->GetGoldPrice();
	const FString PlayerId = GetResolvedPlayerId();

	if (IsPandoraComingSoonProduct(ProductObject, ProductType))
	{
		SetMessage(PandoraComingSoonText);
		RefreshUI();
		SelectEntry(FindEntryDataByProduct(ProductObject, ProductType));
		return false;
	}

	if (!SelectedEntryData->CanSell())
	{
		SetMessage(NotForSaleText);
		return false;
	}

	if (!CanPurchaseProductType(ProductType))
	{
		SetMessage(UnsupportedProductTypeText);
		return false;
	}

	UPlayerProfileSubsystem* ProfileSubsystem = UGameInstance::GetSubsystem<UPlayerProfileSubsystem>(GetGameInstance());
	if (!ProfileSubsystem)
	{

		return false;
	}

	int32 RemainingGold = 0;
	bool bPurchased = false;

	switch (ProductType)
	{
	case EShopProductType::Pandora:
		{
			UPandoraDefinition* PandoraDefinition = Cast<UPandoraDefinition>(ProductObject);
			if (!PandoraDefinition)
			{
				SetMessage(UnsupportedProductTypeText);
				return false;
			}

			if (ProfileSubsystem->IsPandoraGranted(PlayerId, PandoraDefinition))
			{
				SetMessage(AlreadyOwnedText);
				RefreshUI();
				SelectEntry(FindEntryDataByProduct(ProductObject, ProductType));
				return false;
			}

			bPurchased = ProfileSubsystem->TryPurchasePandoraWithGold(
				PlayerId,
				PandoraDefinition,
				GoldPrice,
				PurchasedPandoraStartingLevel,
				RemainingGold,
				false);
			break;
		}
	case EShopProductType::Skin:
		{
			USkinDefinition* SkinDefinition = Cast<USkinDefinition>(ProductObject);
			if (!SkinDefinition)
			{
				SetMessage(UnsupportedProductTypeText);
				return false;
			}

			if (ProfileSubsystem->IsSkinGranted(PlayerId, SkinDefinition))
			{
				SetMessage(AlreadyOwnedText);
				RefreshUI();
				SelectEntry(FindEntryDataByProduct(ProductObject, ProductType));
				return false;
			}

			bPurchased = ProfileSubsystem->TryPurchaseSkinWithGold(
				PlayerId,
				SkinDefinition,
				GoldPrice,
				RemainingGold,
				false);
			break;
		}
	default:
		SetMessage(UnsupportedProductTypeText);
		return false;
	}

	if (!bPurchased)
	{
		SetMessage(NotEnoughGoldText);
		RefreshUI();
		SelectEntry(FindEntryDataByProduct(ProductObject, ProductType));
		return false;
	}

	SetMessage(FText::Format(PurchaseSucceededTextFormat, PurchasedName));
	ProfileSubsystem->SetPreferredSavePlayerId(PlayerId);
	ProfileSubsystem->SaveGame(PlayerId);
	if (APdPlayerController* PdPlayerController = Cast<APdPlayerController>(GetOwningPlayer()))
	{
		PdPlayerController->RequestLocalCosmeticProfileSync();
	}

RefreshUI();
	SelectEntry(FindEntryDataByProduct(ProductObject, ProductType));
	return true;
}

void UShopWidget::HandleCloseClicked()
{
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

	const FString PlayerId = GetResolvedPlayerId();
	if (PlayerId.IsEmpty())
	{

		return;
	}

	UObject* SelectedProductObject = SelectedEntryData ? SelectedEntryData->GetProductObject() : nullptr;
	const EShopProductType SelectedProductType = SelectedEntryData ? SelectedEntryData->GetProductType() : ActiveCategory;

	ProfileSubsystem->SetPreferredSavePlayerId(PlayerId);
	const bool bReset = ProfileSubsystem->ResetShopSaveData(PlayerId, false);
	if (bReset)
	{
		ProfileSubsystem->SaveGame(PlayerId);
	}

RefreshUI();
	if (SelectedProductObject)
	{
		SelectEntry(FindEntryDataByProduct(SelectedProductObject, SelectedProductType));
	}

	if (bReset)
	{
		SetMessage(ShopSaveResetText);
	}
}

void UShopWidget::ResolveWidgets()
{
	if (!TileView)
	{
		TileView = PdWidgetLookup::FindWidgetByNames<UTileView>(this, {
			TEXT("TileView"),
			TEXT("ShopList"),
			TEXT("TileView_Shop"),
			TEXT("TileView_Entries"),
			TEXT("TileView_Pandoras"),
			TEXT("ShopTileView")
		});
	}

	if (!WBP_ShopPreviewPanel)
	{
		WBP_ShopPreviewPanel = PdWidgetLookup::FindWidgetByNames<UShopPreviewPanelWidget>(this, {
			TEXT("WBP_ShopPreviewPanel"),
			TEXT("ShopPreviewPanel"),
			TEXT("PreviewPanel")
		});
	}

	if (!Txt_Gold)
	{
		Txt_Gold = PdWidgetLookup::FindWidgetByNames<UTextBlock>(this, {
			TEXT("Txt_Gold"),
			TEXT("Txt_PlayerGold"),
			TEXT("Txt_GoldAmount")
		});
	}

	if (!Btn_Close)
	{
		Btn_Close = PdWidgetLookup::FindWidgetByNames<UButton>(this, {
			TEXT("Btn_Close"),
			TEXT("Btn_Exit"),
			TEXT("Btn_Back")
		});
	}

	if (!PandoraCategoryButton)
	{
		PandoraCategoryButton = PdWidgetLookup::FindWidgetByNames<UButton>(this, {
			TEXT("PandoraCategoryButton"),
			TEXT("Btn_PandoraCategory"),
			TEXT("Btn_Pandora"),
			TEXT("Btn_PandoraShop")
		});
	}

	if (!SkinCategoryButton)
	{
		SkinCategoryButton = PdWidgetLookup::FindWidgetByNames<UButton>(this, {
			TEXT("SkinCategoryButton"),
			TEXT("Btn_SkinCategory"),
			TEXT("Btn_Skin"),
			TEXT("Btn_SkinShop")
		});
	}

	if (!Btn_ResetShopSave)
	{
		Btn_ResetShopSave = PdWidgetLookup::FindWidgetByNames<UButton>(this, {
			TEXT("Btn_ResetShopSave"),
			TEXT("Btn_ResetShopData"),
			TEXT("Btn_ResetSave"),
			TEXT("Btn_ResetPandoraGold"),
			TEXT("Btn_ResetShopProgress")
		});
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

		UObject* ProductObject = ResolveProductObject(CatalogEntry);
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
		EntryData->OnClicked.AddUObject(this, &ThisClass::HandleEntryDataClicked);
		EntryDataList.Add(EntryData);
	}

}

TArray<FShopCatalogEntry> UShopWidget::BuildEffectiveCatalog() const
{
	TArray<FShopCatalogEntry> EffectiveCatalog;
	TSet<FString> SeenProductKeys;

	const TArray<FShopCatalogProductReference>* ManualProductList = nullptr;
	bool bShouldAutoIncludePandoras = bAutoIncludeAllPandoras;
	bool bShouldAutoIncludeSkins = bAutoIncludeAllSkins;

	if (ShopCatalogDefinition)
	{
		ManualProductList = &ShopCatalogDefinition->ProductList;
		bShouldAutoIncludePandoras = ShopCatalogDefinition->bAutoIncludeAllPandoras;
		bShouldAutoIncludeSkins = ShopCatalogDefinition->bAutoIncludeAllSkins;
	}
	else if (!ShopProductList.IsEmpty())
	{
		ManualProductList = &ShopProductList;
	}

	if (ManualProductList)
	{
		for (const FShopCatalogProductReference& ProductReference : *ManualProductList)
		{
			AppendProductReference(EffectiveCatalog, SeenProductKeys, ProductReference);
		}
	}

	if (bShouldAutoIncludePandoras)
	{
		AppendAllPandoras(EffectiveCatalog, SeenProductKeys);
	}

	if (bShouldAutoIncludeSkins)
	{
		AppendAllSkins(EffectiveCatalog, SeenProductKeys);
	}

	SortCatalogEntries(EffectiveCatalog);
	return EffectiveCatalog;
}

void UShopWidget::AppendProductReference(
	TArray<FShopCatalogEntry>& OutCatalog,
	TSet<FString>& SeenProductKeys,
	const FShopCatalogProductReference& ProductReference) const
{
	if (!ProductReference.HasValidProduct())
	{
		return;
	}

	const FString ProductKey = MakeProductKey(ProductReference);
	if (ProductKey.IsEmpty() || SeenProductKeys.Contains(ProductKey))
	{
		return;
	}

	FShopCatalogEntry CatalogEntry = MakeCatalogEntry(ProductReference);
	if (!ResolveProductObject(CatalogEntry))
	{
		return;
	}

	SeenProductKeys.Add(ProductKey);
	OutCatalog.Add(MoveTemp(CatalogEntry));
}

void UShopWidget::AppendAllPandoras(TArray<FShopCatalogEntry>& OutCatalog, TSet<FString>& SeenProductKeys) const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UContentDataSubsystem* ContentDataSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
	if (!ContentDataSubsystem)
	{
		return;
	}

	TMap<FName, TObjectPtr<UPandoraDefinition>> LoadedPandorasByName;
	ContentDataSubsystem->GetLoadedPandoraDefinitionsByName(LoadedPandorasByName);
	TArray<UPandoraDefinition*> PandoraDefinitions;
	for (const TPair<FName, TObjectPtr<UPandoraDefinition>>& PandoraPair : LoadedPandorasByName)
	{
		PandoraDefinitions.Add(PandoraPair.Value.Get());
	}
	PandoraDefinitions.RemoveAll([](const UPandoraDefinition* PandoraDefinition)
	{
		return !IsValid(PandoraDefinition);
	});

	PandoraDefinitions.Sort([](const UPandoraDefinition& Left, const UPandoraDefinition& Right)
	{
		return Left.GetDisplayName().ToString() < Right.GetDisplayName().ToString();
	});

	for (UPandoraDefinition* PandoraDefinition : PandoraDefinitions)
	{
		FShopCatalogProductReference ProductReference;
		ProductReference.ProductType = EShopProductType::Pandora;
		ProductReference.PandoraDefinition = PandoraDefinition;
		AppendProductReference(OutCatalog, SeenProductKeys, ProductReference);
	}
}

void UShopWidget::AppendAllSkins(TArray<FShopCatalogEntry>& OutCatalog, TSet<FString>& SeenProductKeys) const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UContentDataSubsystem* ContentDataSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
	if (!ContentDataSubsystem)
	{
		return;
	}

	TMap<FName, TObjectPtr<USkinDefinition>> LoadedSkinsByName;
	ContentDataSubsystem->GetLoadedSkinDefinitionsByName(LoadedSkinsByName);
	TArray<USkinDefinition*> SkinDefinitions;
	for (const TPair<FName, TObjectPtr<USkinDefinition>>& SkinPair : LoadedSkinsByName)
	{
		SkinDefinitions.Add(SkinPair.Value.Get());
	}
	SkinDefinitions.RemoveAll([](const USkinDefinition* SkinDefinition)
	{
		return !IsValid(SkinDefinition);
	});

	SkinDefinitions.Sort([](const USkinDefinition& Left, const USkinDefinition& Right)
	{
		return Left.DisplayName.ToString() < Right.DisplayName.ToString();
	});

	for (USkinDefinition* SkinDefinition : SkinDefinitions)
	{
		FShopCatalogProductReference ProductReference;
		ProductReference.ProductType = EShopProductType::Skin;
		ProductReference.SkinDefinition = SkinDefinition;
		AppendProductReference(OutCatalog, SeenProductKeys, ProductReference);
	}
}

FString UShopWidget::MakeProductKey(const FShopCatalogProductReference& ProductReference) const
{
	FSoftObjectPath ObjectPath;
	switch (ProductReference.ProductType)
	{
	case EShopProductType::Pandora:
		ObjectPath = ProductReference.PandoraDefinition.ToSoftObjectPath();
		break;
	case EShopProductType::Skin:
		ObjectPath = ProductReference.SkinDefinition.ToSoftObjectPath();
		break;
	case EShopProductType::Item:
		ObjectPath = ProductReference.ItemDefinition.ToSoftObjectPath();
		break;
	default:
		break;
	}

	if (!ObjectPath.IsValid())
	{
		return FString();
	}

	return FString::Printf(TEXT("%d:%s"), static_cast<int32>(ProductReference.ProductType), *ObjectPath.ToString());
}

void UShopWidget::SortCatalogEntries(TArray<FShopCatalogEntry>& CatalogEntries) const
{
	CatalogEntries.Sort([this](const FShopCatalogEntry& Left, const FShopCatalogEntry& Right)
	{
		if (Left.ShopData.SortOrder != Right.ShopData.SortOrder)
		{
			return Left.ShopData.SortOrder < Right.ShopData.SortOrder;
		}

		if (Left.Product.ProductType != Right.Product.ProductType)
		{
			return static_cast<uint8>(Left.Product.ProductType) < static_cast<uint8>(Right.Product.ProductType);
		}

		return GetCatalogEntrySortName(Left) < GetCatalogEntrySortName(Right);
	});
}

FString UShopWidget::GetCatalogEntrySortName(const FShopCatalogEntry& CatalogEntry) const
{
	UObject* ProductObject = ResolveProductObject(CatalogEntry);
	if (const UPandoraDefinition* PandoraDefinition = Cast<UPandoraDefinition>(ProductObject))
	{
		return PandoraDefinition->GetDisplayName().ToString();
	}

	if (const USkinDefinition* SkinDefinition = Cast<USkinDefinition>(ProductObject))
	{
		return SkinDefinition->DisplayName.ToString();
	}

	if (const UItemDefinition* ItemDefinition = Cast<UItemDefinition>(ProductObject))
	{
		return ItemDefinition->DisplayName.ToString();
	}

	return GetNameSafe(ProductObject);
}

UObject* UShopWidget::ResolveProductObject(const FShopCatalogEntry& CatalogEntry) const
{
	return ResolveProductObject(CatalogEntry.Product);
}

UObject* UShopWidget::ResolveProductObject(const FShopCatalogProductReference& ProductReference) const
{
	switch (ProductReference.ProductType)
	{
	case EShopProductType::Pandora:
		return ProductReference.PandoraDefinition.Get();
	case EShopProductType::Skin:
		return ProductReference.SkinDefinition.Get();
	case EShopProductType::Item:
		return ProductReference.ItemDefinition.Get();
	default:
		return nullptr;
	}
}

FShopCatalogEntry UShopWidget::MakeCatalogEntry(const FShopCatalogProductReference& ProductReference) const
{
	FShopCatalogEntry CatalogEntry;
	CatalogEntry.Product = ProductReference;
	CatalogEntry.ShopData = ResolveShopData(ResolveProductObject(ProductReference), ProductReference.ProductType);
	return CatalogEntry;
}

FShopProductDefinitionData UShopWidget::ResolveShopData(UObject* ProductObject, const EShopProductType ProductType) const
{
	switch (ProductType)
	{
	case EShopProductType::Pandora:
		if (const UPandoraDefinition* PandoraDefinition = Cast<UPandoraDefinition>(ProductObject))
		{
			return PandoraDefinition->ShopData;
		}
		break;
	case EShopProductType::Skin:
		if (const USkinDefinition* SkinDefinition = Cast<USkinDefinition>(ProductObject))
		{
			return SkinDefinition->ShopData;
		}
		break;
	case EShopProductType::Item:
		if (const UItemDefinition* ItemDefinition = Cast<UItemDefinition>(ProductObject))
		{
			return ItemDefinition->ShopData;
		}
		break;
	default:
		break;
	}

	return FShopProductDefinitionData();
}

FString UShopWidget::GetResolvedPlayerId() const
{
	const APlayerController* PlayerController = GetOwningPlayer();
	const APlayerState* PlayerState = PlayerController ? PlayerController->PlayerState : nullptr;

	if (const UPlayerProfileSubsystem* ProfileSubsystem = UGameInstance::GetSubsystem<UPlayerProfileSubsystem>(GetGameInstance()))
	{
		const FString ResolvedPlayerId = ProfileSubsystem->ResolveSavePlayerId(PlayerController, PlayerState);
		if (!ResolvedPlayerId.IsEmpty())
		{
			return ResolvedPlayerId;
		}

		const FString PreferredPlayerId = ProfileSubsystem->GetPreferredSavePlayerId();
		if (!PreferredPlayerId.IsEmpty())
		{
			return PreferredPlayerId;
		}

		return ProfileSubsystem->GetLocalClientSavePlayerId();
	}

	return TEXT("LocalProfile");
}

int32 UShopWidget::GetCurrentGold() const
{
	UPlayerProfileSubsystem* ProfileSubsystem = UGameInstance::GetSubsystem<UPlayerProfileSubsystem>(GetGameInstance());
	return ProfileSubsystem ? ProfileSubsystem->GetGold(GetResolvedPlayerId()) : 0;
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
		return ProfileSubsystem->IsPandoraGranted(GetResolvedPlayerId(), Cast<UPandoraDefinition>(ProductObject));
	case EShopProductType::Skin:
		return ProfileSubsystem->IsSkinGranted(GetResolvedPlayerId(), Cast<USkinDefinition>(ProductObject));
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

	const FString PlayerId = GetResolvedPlayerId();
	if (PlayerId.IsEmpty())
	{
		return;
	}

	ProfileSubsystem->SetPreferredSavePlayerId(PlayerId);
	ProfileSubsystem->LoadGame(PlayerId);

	const UPdSaveGame* SaveGame = ProfileSubsystem->GetOrCreateSaveGame(PlayerId);

}

void UShopWidget::SetMessage(const FText& Message) const
{
	if (WBP_ShopPreviewPanel)
	{
		WBP_ShopPreviewPanel->SetMessage(Message);
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

void UShopWidget::HandleEntryDataClicked(UShopEntryViewData* EntryData)
{
	SelectEntry(EntryData);
}
