#include "UI/Info/Presenter/InfoPandoraTabPresenter.h"

#include "Components/TileView.h"
#include "Data/ContentDataSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Component/Pandora/PandoraComponent.h"
#include "Definition/Item/ItemDefinition.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "Engine/Texture2D.h"
#include "Item/ItemInstance.h"
#include "UI/HUD/PdHUD.h"
#include "Mode/PdPlayerController.h"
#include "Mode/PdPlayerState.h"
#include "Pandora/PandoraLoadoutTypes.h"
#include "UI/Info/InfoLoadoutStore.h"
#include "UI/Info/InfoWidget.h"
#include "UI/Info/Pandora/LeftPandoraWidget.h"
#include "UI/Info/Pandora/PandoraEquipSlotWidget.h"
#include "UI/Info/Pandora/RightPandoraWidget.h"
#include "UI/Pandora/SelectPandoraWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InfoPandoraTabPresenter)

void UInfoPandoraTabPresenter::BindInfoUi(UInfoWidget* InInfoWidget)
{
	if (GetInfoWidget() != InInfoWidget)
	{
		UnbindEvents();
	}

	Super::BindInfoUi(InInfoWidget);
	BindEvents();
}

void UInfoPandoraTabPresenter::Deinitialize()
{
	if (PandoraCatalogPreload.IsValid())
	{
		PandoraCatalogPreload->CancelHandle();
		PandoraCatalogPreload->ReleaseHandle();
		PandoraCatalogPreload.Reset();
	}
	UnbindEvents();
	SelectedEquipSlot = nullptr;
	CurrentFilterTag = FGameplayTag();
	LoadoutStore.Reset();
	bUseTypeFilter = false;
	bShowOnlyOwnedForEquipSlot = false;
	bActive = false;
	Super::Deinitialize();
}

void UInfoPandoraTabPresenter::SetLoadoutStore(UInfoLoadoutStore* InLoadoutStore)
{
	LoadoutStore = InLoadoutStore;
	BindEvents();
}

void UInfoPandoraTabPresenter::SetActive(const bool bInActive)
{
	bActive = bInActive;
	if (!bActive)
	{
		ResetEquipSlotClickState();
		UnbindPandoraTileItemClicked();
	}
}

void UInfoPandoraTabPresenter::Activate()
{
	if (!PandoraCatalogPreload.IsValid())
	{
		const APdPlayerController* Controller = GetController();
		const UGameInstance* GameInstance = Controller ? Controller->GetGameInstance() : nullptr;
		if (UContentDataSubsystem* Content = GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr)
		{
			PandoraCatalogPreload = Content->PreloadPandoraDataAssetsAsync(FSimpleDelegate::CreateUObject(this, &ThisClass::RefreshPandoraTileView));
		}
	}
	bActive = true;
	SelectedEquipSlot = nullptr;
	bShowOnlyOwnedForEquipSlot = false;
	bUseTypeFilter = false;
	CurrentFilterTag = FGameplayTag();
	BindEvents();
	RefreshPandoraTileView();
	BindPandoraTileItemClicked();
	RefreshLoadoutPresentation();
}

void UInfoPandoraTabPresenter::HandleInfoUiOpened()
{
	BindEvents();
	RefreshLoadoutPresentation();
}

void UInfoPandoraTabPresenter::HandleInventoryChanged()
{
	RefreshLoadoutPresentation();
}

void UInfoPandoraTabPresenter::HandleWeaponLoadoutChanged()
{
	RefreshLoadoutPresentation();
}

void UInfoPandoraTabPresenter::HandlePandoraLoadoutChanged()
{
	RefreshLoadoutPresentation();
}

void UInfoPandoraTabPresenter::HandlePresentationAssetsReady()
{
	RefreshLoadoutPresentation();
}

void UInfoPandoraTabPresenter::RefreshLoadoutPresentation() const
{
	RefreshLeftPandoraSlots();
	RefreshSelectPandoraLoadout();
}

void UInfoPandoraTabPresenter::HandlePandoraSlotClicked(UObject* Item)
{
	const UPandoraDefinition* PandoraDefinition = Cast<UPandoraDefinition>(Item);
	if (!GetController() || !IsPandoraOwned(PandoraDefinition))
	{
		return;
	}

	UInfoLoadoutStore* Store = LoadoutStore.Get();
	UPandoraComponent* PandoraComponent = Store ? Store->GetPandoraComponent() : nullptr;
	EEnum_Direction Direction = EEnum_Direction::Center;
	int32 SlotNumber = 0;
	if (!ResolveLoadoutSlotForClick(PandoraComponent, PandoraDefinition, Direction, SlotNumber))
	{
		return;
	}

	if (Store && PandoraComponent)
	{
		const bool bRequested = Store->RequestSetPandoraLoadoutSlot(
			Direction,
			PandoraDefinition);
		if (bRequested)
		{
			ResetEquipSlotClickState();
		}
	}
}

void UInfoPandoraTabPresenter::HandlePandoraEquipSlotClicked(
	UPandoraEquipSlotWidget* InSelectedEquipSlot,
	const bool bIsSelectedAnyButton)
{
	if (!GetController())
	{
		return;
	}
	if (InSelectedEquipSlot && InSelectedEquipSlot->GetCachedData())
	{
		ClearPandoraEquipSlot(InSelectedEquipSlot);
		return;
	}

	SelectedEquipSlot = InSelectedEquipSlot;
	if (SelectedEquipSlot)
	{
		SelectedEquipSlot->ToggleText_Apply(!bIsSelectedAnyButton);
	}
	if (bIsSelectedAnyButton)
	{
		SelectedEquipSlot = nullptr;
		bShowOnlyOwnedForEquipSlot = false;
		RefreshPandoraTileView();
		return;
	}

	bShowOnlyOwnedForEquipSlot = true;
	RefreshPandoraTileView();
	BindPandoraTileItemClicked();
}

void UInfoPandoraTabPresenter::HandlePandoraFilterTypeClicked(const FGameplayTag TypeTag)
{
	if (!GetController())
	{
		return;
	}
	bUseTypeFilter = TypeTag.IsValid();
	CurrentFilterTag = bUseTypeFilter ? TypeTag : FGameplayTag();
	RefreshPandoraTileView();
}

void UInfoPandoraTabPresenter::HandlePandoraFilterAllClicked()
{
	bUseTypeFilter = false;
	CurrentFilterTag = FGameplayTag();
	RefreshPandoraTileView();
}

void UInfoPandoraTabPresenter::HandlePandoraInventoryChanged()
{
	if (bActive)
	{
		RefreshPandoraTileView();
	}
	RefreshLoadoutPresentation();
}

void UInfoPandoraTabPresenter::BindEvents()
{
	const APdPlayerState* PlayerState = GetPlayerState();
	UPandoraComponent* PandoraComponent = PlayerState ? PlayerState->GetPandoraComponent() : nullptr;
	if (BoundPandoraComponent.Get() != PandoraComponent)
	{
		if (BoundPandoraComponent.IsValid())
		{
			BoundPandoraComponent->OnPandoraInventoryChanged.RemoveDynamic(this, &ThisClass::HandlePandoraInventoryChanged);
		}
		BoundPandoraComponent = PandoraComponent;
	}
	if (PandoraComponent)
	{
		PandoraComponent->OnPandoraInventoryChanged.AddUniqueDynamic(this, &ThisClass::HandlePandoraInventoryChanged);
	}
	UInfoWidget* InfoWidget = GetInfoWidget();
	if (!InfoWidget)
	{
		return;
	}

	if (ULeftPandoraWidget* LeftPandoraWidget = InfoWidget->GetLeftPandoraWidget())
	{
		LeftPandoraWidget->OnClicked_PandoraEquipSlot.RemoveDynamic(
			this,
			&ThisClass::HandlePandoraEquipSlotClicked);
		LeftPandoraWidget->OnClicked_PandoraEquipSlot.AddUniqueDynamic(
			this,
			&ThisClass::HandlePandoraEquipSlotClicked);
	}
	if (URightPandoraWidget* RightPandoraWidget = InfoWidget->GetRightPandoraWidget())
	{
		RightPandoraWidget->OnClicked_FilterAllButton.RemoveDynamic(
			this,
			&ThisClass::HandlePandoraFilterAllClicked);
		RightPandoraWidget->OnClicked_FilterAllButton.AddUniqueDynamic(
			this,
			&ThisClass::HandlePandoraFilterAllClicked);
		RightPandoraWidget->OnClicked_FilterTypeButton.RemoveDynamic(
			this,
			&ThisClass::HandlePandoraFilterTypeClicked);
		RightPandoraWidget->OnClicked_FilterTypeButton.AddUniqueDynamic(
			this,
			&ThisClass::HandlePandoraFilterTypeClicked);
	}
}

void UInfoPandoraTabPresenter::UnbindEvents()
{
	if (BoundPandoraComponent.IsValid())
	{
		BoundPandoraComponent->OnPandoraInventoryChanged.RemoveDynamic(this, &ThisClass::HandlePandoraInventoryChanged);
	}
	BoundPandoraComponent.Reset();
	UnbindPandoraTileItemClicked();
	UInfoWidget* InfoWidget = GetInfoWidget();
	if (!InfoWidget)
	{
		return;
	}
	if (ULeftPandoraWidget* LeftPandoraWidget = InfoWidget->GetLeftPandoraWidget())
	{
		LeftPandoraWidget->OnClicked_PandoraEquipSlot.RemoveDynamic(
			this,
			&ThisClass::HandlePandoraEquipSlotClicked);
	}
	if (URightPandoraWidget* RightPandoraWidget = InfoWidget->GetRightPandoraWidget())
	{
		RightPandoraWidget->OnClicked_FilterAllButton.RemoveDynamic(
			this,
			&ThisClass::HandlePandoraFilterAllClicked);
		RightPandoraWidget->OnClicked_FilterTypeButton.RemoveDynamic(
			this,
			&ThisClass::HandlePandoraFilterTypeClicked);
	}
}

USelectPandoraWidget* UInfoPandoraTabPresenter::GetSelectPandoraWidget() const
{
	const APdPlayerController* Controller = GetController();
	const APdHUD* HUD = Controller ? Cast<APdHUD>(Controller->GetHUD()) : nullptr;
	return HUD ? HUD->GetSelectPandoraWidget() : nullptr;
}

UItemInstance* UInfoPandoraTabPresenter::GetSelectedWeapon(const EEnum_Direction Direction) const
{
	const UInfoLoadoutStore* Store = LoadoutStore.Get();
	return Store ? Store->GetSelectedWeapon(Direction) : nullptr;
}

void UInfoPandoraTabPresenter::RefreshLeftPandoraSlots() const
{
	UInfoWidget* InfoWidget = GetInfoWidget();
	ULeftPandoraWidget* LeftPandoraWidget = InfoWidget ? InfoWidget->GetLeftPandoraWidget() : nullptr;
	if (!LeftPandoraWidget)
	{
		return;
	}

	const UInfoLoadoutStore* Store = LoadoutStore.Get();
	UPandoraComponent* PandoraComponent = Store ? Store->GetPandoraComponent() : nullptr;
	LeftPandoraWidget->RefreshPandoraLoadoutSlots(PandoraComponent);

	const auto ResolveWeaponIcon = [](const UItemInstance* WeaponInstance) -> UTexture2D*
	{
		const UItemDefinition* WeaponDefinition = IsValid(WeaponInstance)
			? WeaponInstance->ItemDefinition.Get()
			: nullptr;
		return WeaponDefinition ? WeaponDefinition->IconTexture.Get() : nullptr;
	};
	LeftPandoraWidget->SetWeaponImage(1, ResolveWeaponIcon(GetSelectedWeapon(EEnum_Direction::Left)));
	LeftPandoraWidget->SetWeaponImage(2, ResolveWeaponIcon(GetSelectedWeapon(EEnum_Direction::Up)));
	LeftPandoraWidget->SetWeaponImage(3, ResolveWeaponIcon(GetSelectedWeapon(EEnum_Direction::Right)));
}

void UInfoPandoraTabPresenter::RefreshSelectPandoraLoadout() const
{
	USelectPandoraWidget* SelectPandoraWidget = GetSelectPandoraWidget();
	if (!SelectPandoraWidget)
	{
		return;
	}
	const UInfoLoadoutStore* Store = LoadoutStore.Get();
	const UPandoraComponent* PandoraComponent = Store ? Store->GetPandoraComponent() : nullptr;
	for (const EEnum_Direction Direction :
		{EEnum_Direction::Left, EEnum_Direction::Up, EEnum_Direction::Right})
	{
		const int32 SlotNumber = PandoraLoadout::GetLoadoutNumberFromDirection(Direction);
		const UPandoraDefinition* PandoraDefinition = PandoraComponent
			? PandoraComponent->GetPandoraLoadoutDefinition(Direction) : nullptr;
		const UItemInstance* Weapon = GetSelectedWeapon(Direction);
		const UItemDefinition* WeaponDefinition = IsValid(Weapon) ? Weapon->ItemDefinition.Get() : nullptr;
		SelectPandoraWidget->SetPandoraImage(
			SlotNumber, PandoraDefinition ? PandoraDefinition->GetIconTexture() : nullptr);
		SelectPandoraWidget->SetWeaponImage(
			SlotNumber, WeaponDefinition ? WeaponDefinition->IconTexture.Get() : nullptr);
		SelectPandoraWidget->SetPandoraEnabled(
			SlotNumber, !PandoraDefinition || PandoraDefinition->IsCompatibleWithWeaponDefinition(WeaponDefinition));
	}
}

bool UInfoPandoraTabPresenter::IsPandoraOwned(const UPandoraDefinition* PandoraDefinition) const
{
	const APdPlayerState* PlayerState = GetPlayerState();
	const UPandoraComponent* PandoraComponent = PlayerState ? PlayerState->GetPandoraComponent() : nullptr;
	return IsValid(PandoraDefinition) && PandoraComponent && PandoraComponent->HasPandoraDefinition(PandoraDefinition);
}

void UInfoPandoraTabPresenter::BuildPandoraTileViewItems(
	TArray<UObject*>& OutListItems, const FGameplayTag TypeTag, const bool bOwnedOnly) const
{
	OutListItems.Reset();
	const APdPlayerController* Controller = GetController();
	const UGameInstance* GameInstance = Controller ? Controller->GetGameInstance() : nullptr;
	const UContentDataSubsystem* Content = GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
	if (!Content)
	{
		return;
	}
	TMap<FName, TObjectPtr<UPandoraDefinition>> Catalog;
	Content->GetLoadedPandoraDefinitionsByName(Catalog);
	for (const auto& Pair : Catalog)
	{
		UPandoraDefinition* Definition = Pair.Value;
		if (IsValid(Definition) && (!TypeTag.IsValid() || Definition->GetIdTag().MatchesTag(TypeTag))
			&& (!bOwnedOnly || IsPandoraOwned(Definition)))
		{
			OutListItems.Add(Definition);
		}
	}
	OutListItems.Sort([](const UObject& Left, const UObject& Right)
	{
		return Left.GetName() < Right.GetName();
	});
}

void UInfoPandoraTabPresenter::RefreshPandoraTileView() const
{
	TArray<UObject*> CurrentPandoraList;
	BuildPandoraTileViewItems(
		CurrentPandoraList,
		bUseTypeFilter ? CurrentFilterTag : FGameplayTag(),
		bShowOnlyOwnedForEquipSlot);
	UInfoWidget* InfoWidget = GetInfoWidget();
	if (URightPandoraWidget* RightPandoraWidget = InfoWidget ? InfoWidget->GetRightPandoraWidget() : nullptr)
	{
		RightPandoraWidget->SetTileView(CurrentPandoraList);
	}
}

void UInfoPandoraTabPresenter::BindPandoraTileItemClicked()
{
	UInfoWidget* InfoWidget = GetInfoWidget();
	URightPandoraWidget* RightPandoraWidget = InfoWidget ? InfoWidget->GetRightPandoraWidget() : nullptr;
	UTileView* TileView = RightPandoraWidget ? RightPandoraWidget->GetTileView() : nullptr;
	if (!TileView)
	{
		return;
	}
	if (BoundTileView.Get() == TileView && TileItemClickedDelegateHandle.IsValid())
	{
		return;
	}

	UnbindPandoraTileItemClicked();
	BoundTileView = TileView;
	TileItemClickedDelegateHandle =
		TileView->OnItemClicked().AddUObject(this, &ThisClass::HandlePandoraSlotClicked);
}

void UInfoPandoraTabPresenter::UnbindPandoraTileItemClicked()
{
	if (UTileView* TileView = BoundTileView.Get())
	{
		if (TileItemClickedDelegateHandle.IsValid())
		{
			TileView->OnItemClicked().Remove(TileItemClickedDelegateHandle);
		}
	}
	BoundTileView.Reset();
	TileItemClickedDelegateHandle.Reset();
}

bool UInfoPandoraTabPresenter::ResolveLoadoutSlotForClick(
	const UPandoraComponent* PandoraComponent,
	const UPandoraDefinition* PandoraDefinition,
	EEnum_Direction& OutDirection,
	int32& OutSlotNumber) const
{
	OutDirection = EEnum_Direction::Center;
	OutSlotNumber = 0;
	if (!PandoraComponent || !PandoraDefinition)
	{
		return false;
	}

	if (SelectedEquipSlot)
	{
		const int32 SelectedSlotNumber = SelectedEquipSlot->GetNth();
		const EEnum_Direction SelectedDirection =
			PandoraLoadout::GetDirectionFromLoadoutNumber(SelectedSlotNumber);
		if (PandoraLoadout::IsLoadoutDirection(SelectedDirection))
		{
			OutDirection = SelectedDirection;
			OutSlotNumber = SelectedSlotNumber;
			return true;
		}
	}

	for (const EEnum_Direction Direction :
		{ EEnum_Direction::Left, EEnum_Direction::Up, EEnum_Direction::Right })
	{
		if (PandoraComponent->GetPandoraLoadoutDefinition(Direction) == PandoraDefinition)
		{
			OutDirection = Direction;
			OutSlotNumber = PandoraLoadout::GetLoadoutNumberFromDirection(Direction);
			return true;
		}
	}
	for (const EEnum_Direction Direction :
		{ EEnum_Direction::Left, EEnum_Direction::Up, EEnum_Direction::Right })
	{
		if (!PandoraComponent->GetPandoraLoadoutDefinition(Direction))
		{
			OutDirection = Direction;
			OutSlotNumber = PandoraLoadout::GetLoadoutNumberFromDirection(Direction);
			return true;
		}
	}
	return false;
}

void UInfoPandoraTabPresenter::ResetEquipSlotClickState()
{
	if (SelectedEquipSlot)
	{
		SelectedEquipSlot->ToggleText_Apply(false);
	}
	SelectedEquipSlot = nullptr;
	bShowOnlyOwnedForEquipSlot = false;

	if (UInfoWidget* InfoWidget = GetInfoWidget())
	{
		if (ULeftPandoraWidget* LeftPandoraWidget = InfoWidget->GetLeftPandoraWidget())
		{
			LeftPandoraWidget->ClearPandoraEquipSlotSelection();
		}
	}
	if (bActive)
	{
		RefreshPandoraTileView();
	}
}

void UInfoPandoraTabPresenter::ClearPandoraEquipSlot(
	UPandoraEquipSlotWidget* TargetPandoraEquipSlot)
{
	if (!TargetPandoraEquipSlot)
	{
		return;
	}
	const EEnum_Direction Direction = PandoraLoadout::GetDirectionFromLoadoutNumber(
		TargetPandoraEquipSlot->GetNth());
	if (!PandoraLoadout::IsLoadoutDirection(Direction))
	{
		return;
	}

	UInfoLoadoutStore* Store = LoadoutStore.Get();
	if (!Store || !Store->GetPandoraComponent())
	{
		return;
	}
	if (!Store->RequestSetPandoraLoadoutSlot(Direction, nullptr))
	{
		return;
	}

	if (SelectedEquipSlot == TargetPandoraEquipSlot)
	{
		SelectedEquipSlot = nullptr;
	}
	bShowOnlyOwnedForEquipSlot = false;
	TargetPandoraEquipSlot->ToggleText_Apply(false);
	if (UInfoWidget* InfoWidget = GetInfoWidget())
	{
		if (ULeftPandoraWidget* LeftPandoraWidget = InfoWidget->GetLeftPandoraWidget())
		{
			LeftPandoraWidget->ClearPandoraEquipSlotSelection();
		}
	}
	RefreshPandoraTileView();
}
