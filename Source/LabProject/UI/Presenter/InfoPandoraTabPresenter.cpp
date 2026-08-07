#include "UI/Presenter/InfoPandoraTabPresenter.h"

#include "Components/TileView.h"
#include "Component/AbilitySystem/PandoraTreeComponent.h"
#include "Component/Pandora/PandoraComponent.h"
#include "Definition/Item/ItemDefinition.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "Engine/Texture2D.h"
#include "Item/ItemInstance.h"
#include "Mode/PdHUD.h"
#include "Mode/PdPlayerController.h"
#include "Mode/PdPlayerState.h"
#include "Pandora/PandoraInstance.h"
#include "Pandora/PandoraLoadoutTypes.h"
#include "UI/InfoLoadoutStore.h"
#include "UI/PandoraLoadoutUiModel.h"
#include "UI/Widget/InfoWidget.h"
#include "UI/Widget/LeftPandoraWidget.h"
#include "UI/Widget/PandoraEquipSlotWidget.h"
#include "UI/Widget/RightPandoraWidget.h"
#include "UI/Widget/SelectPandoraWidget.h"

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
	RefreshSelectPandoraImages();
	RefreshSelectPandoraCompatibility();
}

void UInfoPandoraTabPresenter::HandlePandoraSlotClicked(UObject* Item)
{
	UPandoraInstance* PandoraInstance = Cast<UPandoraInstance>(Item);
	if (!GetController() || !IsPandoraOwned(PandoraInstance))
	{
		return;
	}

	const UPandoraDefinition* PandoraDefinition = PandoraInstance
		? PandoraInstance->PandoraDefinition.Get()
		: nullptr;
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

void UInfoPandoraTabPresenter::BindEvents()
{
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
		RightPandoraWidget->OnClicked_PandoraFilterAllButton.RemoveDynamic(
			this,
			&ThisClass::HandlePandoraFilterAllClicked);
		RightPandoraWidget->OnClicked_PandoraFilterAllButton.AddUniqueDynamic(
			this,
			&ThisClass::HandlePandoraFilterAllClicked);
		RightPandoraWidget->OnClicked_PandoraFilterTypeButton.RemoveDynamic(
			this,
			&ThisClass::HandlePandoraFilterTypeClicked);
		RightPandoraWidget->OnClicked_PandoraFilterTypeButton.AddUniqueDynamic(
			this,
			&ThisClass::HandlePandoraFilterTypeClicked);
	}
}

void UInfoPandoraTabPresenter::UnbindEvents()
{
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
		RightPandoraWidget->OnClicked_PandoraFilterAllButton.RemoveDynamic(
			this,
			&ThisClass::HandlePandoraFilterAllClicked);
		RightPandoraWidget->OnClicked_PandoraFilterTypeButton.RemoveDynamic(
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
	const UPandoraComponent* PandoraComponent = Store ? Store->GetPandoraComponent() : nullptr;
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

void UInfoPandoraTabPresenter::RefreshSelectPandoraImages() const
{
	USelectPandoraWidget* SelectPandoraWidget = GetSelectPandoraWidget();
	if (!SelectPandoraWidget)
	{
		return;
	}

	const UInfoLoadoutStore* Store = LoadoutStore.Get();
	const UPandoraComponent* PandoraComponent = Store ? Store->GetPandoraComponent() : nullptr;
	const UItemInstance* LeftWeapon = GetSelectedWeapon(EEnum_Direction::Left);
	const UItemInstance* UpWeapon = GetSelectedWeapon(EEnum_Direction::Up);
	const UItemInstance* RightWeapon = GetSelectedWeapon(EEnum_Direction::Right);
	const TArray<FPandoraSelectSlotUiData> Slots = FPandoraLoadoutUiModel::BuildSelectSlots(
		PandoraComponent,
		LeftWeapon,
		UpWeapon,
		RightWeapon);
	for (const FPandoraSelectSlotUiData& Slot : Slots)
	{
		SelectPandoraWidget->SetPandoraImage(Slot.SlotNumber, Slot.IconTexture);
	}

	const auto ResolveWeaponIcon = [](const UItemInstance* WeaponInstance) -> UTexture2D*
	{
		const UItemDefinition* WeaponDefinition = IsValid(WeaponInstance)
			? WeaponInstance->ItemDefinition.Get()
			: nullptr;
		return WeaponDefinition ? WeaponDefinition->IconTexture.Get() : nullptr;
	};
	SelectPandoraWidget->SetWeaponImage(1, ResolveWeaponIcon(LeftWeapon));
	SelectPandoraWidget->SetWeaponImage(2, ResolveWeaponIcon(UpWeapon));
	SelectPandoraWidget->SetWeaponImage(3, ResolveWeaponIcon(RightWeapon));
}

void UInfoPandoraTabPresenter::RefreshSelectPandoraCompatibility() const
{
	USelectPandoraWidget* SelectPandoraWidget = GetSelectPandoraWidget();
	if (!SelectPandoraWidget)
	{
		return;
	}
	const UInfoLoadoutStore* Store = LoadoutStore.Get();
	const UPandoraComponent* PandoraComponent = Store ? Store->GetPandoraComponent() : nullptr;
	const TArray<FPandoraSelectSlotUiData> Slots = FPandoraLoadoutUiModel::BuildSelectSlots(
		PandoraComponent,
		GetSelectedWeapon(EEnum_Direction::Left),
		GetSelectedWeapon(EEnum_Direction::Up),
		GetSelectedWeapon(EEnum_Direction::Right));
	for (const FPandoraSelectSlotUiData& Slot : Slots)
	{
		SelectPandoraWidget->SetPandoraEnabled(Slot.SlotNumber, Slot.bCompatibleWithWeapon);
	}
}

bool UInfoPandoraTabPresenter::IsPandoraOwned(const UPandoraInstance* PandoraInstance) const
{
	if (!IsValid(PandoraInstance))
	{
		return false;
	}
	UPandoraDefinition* PandoraDefinition =
		const_cast<UPandoraDefinition*>(PandoraInstance->PandoraDefinition.Get());
	if (!IsValid(PandoraDefinition))
	{
		return false;
	}
	const APdPlayerState* PlayerState = GetPlayerState();
	const UPandoraTreeComponent* PandoraTree = PlayerState
		? PlayerState->GetPandoraTreeComponent()
		: nullptr;
	return PandoraTree
		? PandoraTree->IsPandoraUnlockedForTree(PandoraDefinition)
		: PandoraInstance->IsOwned;
}

void UInfoPandoraTabPresenter::BuildPandoraTileViewItems(
	TArray<UObject*>& OutListItems,
	const FGameplayTag TypeTag,
	const bool bOwnedOnly) const
{
	OutListItems.Reset();
	const UInfoLoadoutStore* Store = LoadoutStore.Get();
	const UPandoraComponent* PandoraComponent = Store ? Store->GetPandoraComponent() : nullptr;
	if (!PandoraComponent)
	{
		return;
	}

	const FPandoraList* SourceList = TypeTag.IsValid()
		? PandoraComponent->Map_Type_PandoraList.Find(TypeTag)
		: &PandoraComponent->AllPandoraList;
	if (!SourceList)
	{
		return;
	}

	OutListItems.Reserve(SourceList->Pandoras.Num());
	for (const TObjectPtr<UPandoraInstance>& Pandora : SourceList->Pandoras)
	{
		UPandoraInstance* PandoraInstance = Pandora.Get();
		if (IsValid(PandoraInstance) && (!bOwnedOnly || IsPandoraOwned(PandoraInstance)))
		{
			OutListItems.Add(PandoraInstance);
		}
	}
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
		RightPandoraWidget->SetTileViewAndShowLockState(CurrentPandoraList);
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
			FPandoraLoadoutUiModel::GetDirectionFromSelectSlotNumber(SelectedSlotNumber);
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
			OutSlotNumber = FPandoraLoadoutUiModel::GetSelectSlotNumberFromDirection(Direction);
			return true;
		}
	}
	for (const EEnum_Direction Direction :
		{ EEnum_Direction::Left, EEnum_Direction::Up, EEnum_Direction::Right })
	{
		if (!PandoraComponent->GetPandoraLoadoutDefinition(Direction))
		{
			OutDirection = Direction;
			OutSlotNumber = FPandoraLoadoutUiModel::GetSelectSlotNumberFromDirection(Direction);
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
	const EEnum_Direction Direction = FPandoraLoadoutUiModel::GetDirectionFromSelectSlotNumber(
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
