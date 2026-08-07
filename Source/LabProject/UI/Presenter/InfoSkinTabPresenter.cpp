#include "UI/Presenter/InfoSkinTabPresenter.h"

#include "Character/PdPlayer.h"
#include "Common/LabGameplayTags.h"
#include "Components/TileView.h"
#include "Component/Skin/SkinComponent.h"
#include "Component/Skin/SkinEquipmentComponent.h"
#include "Definition/Skin/SkinDefinition.h"
#include "Mode/PdPlayerController.h"
#include "Mode/PdPlayerState.h"
#include "Skin/SkinInstance.h"
#include "UI/Widget/InfoWidget.h"
#include "UI/Widget/LeftSkinWidget.h"
#include "UI/Widget/RightSkinWidget.h"
#include "UI/Widget/SkinEquipSlotWidget.h"
#include "UI/Widget/SkinSlotViewData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InfoSkinTabPresenter)

namespace
{
void AppendSkinListAsObjects(const FSkinList& SkinList, TArray<UObject*>& OutListItems)
{
	OutListItems.Reserve(OutListItems.Num() + SkinList.Skins.Num());
	for (const TObjectPtr<USkinInstance>& Skin : SkinList.Skins)
	{
		if (USkinInstance* SkinInstance = Skin.Get())
		{
			OutListItems.Add(SkinInstance);
		}
	}
}

FGameplayTag ResolveSkinDefinitionMatchTag(const FGameplayTag SlotOrFilterTag)
{
	return SlotOrFilterTag.MatchesTag(LabGameplayTags::Skin_Gesture)
		? LabGameplayTags::Skin_Gesture
		: SlotOrFilterTag;
}
}

void UInfoSkinTabPresenter::BindInfoUi(UInfoWidget* InInfoWidget)
{
	if (GetInfoWidget() != InInfoWidget)
	{
		UnbindEvents();
	}

	Super::BindInfoUi(InInfoWidget);
	BindEvents();
}

void UInfoSkinTabPresenter::Deinitialize()
{
	UnbindEvents();
	SelectedEquipSlot = nullptr;
	SelectedEquipTypeTag = FGameplayTag();
	Super::Deinitialize();
}

void UInfoSkinTabPresenter::Activate()
{
	SelectedEquipSlot = nullptr;
	SelectedEquipTypeTag = FGameplayTag();
	BindEvents();
	PopulateAllSkins();

	if (UInfoWidget* InfoWidget = GetInfoWidget())
	{
		if (URightSkinWidget* RightSkinWidget = InfoWidget->GetRightSkinWidget())
		{
			RightSkinWidget->ToggleActiveFiliterButtons(true);
		}

		if (ULeftSkinWidget* LeftSkinWidget = InfoWidget->GetLeftSkinWidget())
		{
			LeftSkinWidget->InitialzeEquipSlots();
		}
	}

	RefreshEquippedSlots();
}

void UInfoSkinTabPresenter::HandleInfoUiOpened()
{
	BindEvents();
	if (UInfoWidget* InfoWidget = GetInfoWidget())
	{
		if (ULeftSkinWidget* LeftSkinWidget = InfoWidget->GetLeftSkinWidget())
		{
			LeftSkinWidget->InitialzeEquipSlots();
		}
	}
	RefreshEquippedSlots();
}

void UInfoSkinTabPresenter::RefreshEquippedSlots() const
{
	UInfoWidget* InfoWidget = GetInfoWidget();
	ULeftSkinWidget* LeftSkinWidget = InfoWidget ? InfoWidget->GetLeftSkinWidget() : nullptr;
	if (!LeftSkinWidget)
	{
		return;
	}

	const APdPlayerController* Controller = GetController();
	const APdPlayer* Player = Controller ? Cast<APdPlayer>(Controller->GetPawn()) : nullptr;
	const USkinEquipmentComponent* SkinEquipment = Player ? Player->GetSkinEquipmentComponent() : nullptr;
	LeftSkinWidget->RefreshEquippedSkinSlots(SkinEquipment);
}

void UInfoSkinTabPresenter::HandleSkinSlotClicked(UObject* Item)
{
	USkinSlotViewData* SlotViewData = Cast<USkinSlotViewData>(Item);
	USkinInstance* SkinInstance = Cast<USkinInstance>(Item);
	if (!SkinInstance && SlotViewData)
	{
		SkinInstance = SlotViewData->GetSkinInstance();
	}

	const USkinDefinition* SkinDefinition = IsValid(SkinInstance)
		? SkinInstance->SkinDefinition.Get()
		: nullptr;
	if (!GetController()
		|| !SkinDefinition
		|| !SkinDefinition->IdTag.IsValid()
		|| !SelectedEquipSlot
		|| !SelectedEquipTypeTag.IsValid())
	{
		return;
	}

	const FGameplayTag RequiredSkinTag = ResolveSkinDefinitionMatchTag(SelectedEquipTypeTag);
	if (!SkinDefinition->IdTag.MatchesTag(RequiredSkinTag))
	{
		return;
	}

	SelectedEquipSlot->SetData(SkinInstance);
	ClearTileItemClicked();

	APdPlayer* Player = Cast<APdPlayer>(GetController()->GetPawn());
	USkinEquipmentComponent* SkinEquipment = Player ? Player->GetSkinEquipmentComponent() : nullptr;
	if (SkinEquipment)
	{
		const bool bRequested = SkinEquipment->RequestEquipSkin(SkinInstance, SelectedEquipTypeTag);
		if (!bRequested || (SkinEquipment->GetOwner() && SkinEquipment->GetOwner()->HasAuthority()))
		{
			RefreshEquippedSlots();
		}
	}
}

void UInfoSkinTabPresenter::HandleSkinEquipSlotClicked(
	FGameplayTag EquipTypeTag,
	USkinEquipSlotWidget* InSelectedEquipSlot,
	const bool bIsSelectedAnyButton)
{
	static_cast<void>(bIsSelectedAnyButton);
	if (!GetController())
	{
		return;
	}

	ClearTileItemClicked();
	SelectedEquipSlot = nullptr;
	SelectedEquipTypeTag = FGameplayTag();
	if (!InSelectedEquipSlot || !EquipTypeTag.IsValid())
	{
		return;
	}

	if (InSelectedEquipSlot->HasEquippedSkin())
	{
		ClearSkinEquipSlot(InSelectedEquipSlot, EquipTypeTag);
		return;
	}

	SelectedEquipSlot = InSelectedEquipSlot;
	SelectedEquipTypeTag = EquipTypeTag;
	HandleSkinFilterTypeClicked(ResolveSkinDefinitionMatchTag(EquipTypeTag));
	BindTileItemClicked();
}

void UInfoSkinTabPresenter::HandleSkinEquipSlotDropped(
	const FGameplayTag EquipTypeTag,
	USkinEquipSlotWidget* TargetSkinEquipSlot,
	USkinInstance* SkinInstance)
{
	SelectedEquipSlot = TargetSkinEquipSlot;
	SelectedEquipTypeTag = EquipTypeTag;
	HandleSkinSlotClicked(SkinInstance);
}

void UInfoSkinTabPresenter::HandleSkinDroppedToCharacter(USkinInstance* SkinInstance)
{
	UInfoWidget* InfoWidget = GetInfoWidget();
	ULeftSkinWidget* LeftSkinWidget = InfoWidget ? InfoWidget->GetLeftSkinWidget() : nullptr;
	USkinEquipSlotWidget* TargetSlot = LeftSkinWidget
		? LeftSkinWidget->FindFirstCompatibleSkinEquipSlot(SkinInstance)
		: nullptr;
	if (!TargetSlot)
	{
		return;
	}

	SelectedEquipSlot = TargetSlot;
	SelectedEquipTypeTag = TargetSlot->GetAcceptedEquipTypeTag();
	HandleSkinSlotClicked(SkinInstance);
}

void UInfoSkinTabPresenter::HandleSkinFilterTypeClicked(FGameplayTag TypeTag)
{
	TypeTag = ResolveSkinDefinitionMatchTag(TypeTag);
	if (!GetController())
	{
		return;
	}

	TArray<UObject*> CurrentSkinList;
	const APdPlayerState* PlayerState = GetPlayerState();
	const USkinComponent* SkinComponent = PlayerState ? PlayerState->GetSkinComponent() : nullptr;
	if (SkinComponent)
	{
		if (const FSkinList* FoundSkinList = SkinComponent->Map_Type_SkinList.Find(TypeTag))
		{
			AppendSkinListAsObjects(*FoundSkinList, CurrentSkinList);
		}
		else if (TypeTag.IsValid())
		{
			for (USkinInstance* SkinInstance : SkinComponent->AllSkinList.Skins)
			{
				const USkinDefinition* SkinDefinition = IsValid(SkinInstance)
					? SkinInstance->SkinDefinition.Get()
					: nullptr;
				if (SkinDefinition && SkinDefinition->IdTag.MatchesTag(TypeTag))
				{
					CurrentSkinList.Add(SkinInstance);
				}
			}
		}
	}

	UInfoWidget* InfoWidget = GetInfoWidget();
	if (URightSkinWidget* RightSkinWidget = InfoWidget ? InfoWidget->GetRightSkinWidget() : nullptr)
	{
		RightSkinWidget->SetTileView(CurrentSkinList);
	}
}

void UInfoSkinTabPresenter::HandleSkinFilterAllClicked()
{
	PopulateAllSkins();
}

void UInfoSkinTabPresenter::BindEvents()
{
	UInfoWidget* InfoWidget = GetInfoWidget();
	if (!InfoWidget)
	{
		return;
	}

	InfoWidget->OnDroppedSkinToCharacterPanel.RemoveDynamic(
		this,
		&ThisClass::HandleSkinDroppedToCharacter);
	InfoWidget->OnDroppedSkinToCharacterPanel.AddUniqueDynamic(
		this,
		&ThisClass::HandleSkinDroppedToCharacter);

	if (ULeftSkinWidget* LeftSkinWidget = InfoWidget->GetLeftSkinWidget())
	{
		LeftSkinWidget->OnClicked_SkinEquipTypeSlot.RemoveDynamic(
			this,
			&ThisClass::HandleSkinEquipSlotClicked);
		LeftSkinWidget->OnClicked_SkinEquipTypeSlot.AddUniqueDynamic(
			this,
			&ThisClass::HandleSkinEquipSlotClicked);
		LeftSkinWidget->OnDroppedSkin_SkinEquipTypeSlot.RemoveDynamic(
			this,
			&ThisClass::HandleSkinEquipSlotDropped);
		LeftSkinWidget->OnDroppedSkin_SkinEquipTypeSlot.AddUniqueDynamic(
			this,
			&ThisClass::HandleSkinEquipSlotDropped);
	}

	if (URightSkinWidget* RightSkinWidget = InfoWidget->GetRightSkinWidget())
	{
		RightSkinWidget->OnClicked_SkinFilterAllButton.RemoveDynamic(
			this,
			&ThisClass::HandleSkinFilterAllClicked);
		RightSkinWidget->OnClicked_SkinFilterAllButton.AddUniqueDynamic(
			this,
			&ThisClass::HandleSkinFilterAllClicked);
		RightSkinWidget->OnClicked_SkinFilterTypeButton.RemoveDynamic(
			this,
			&ThisClass::HandleSkinFilterTypeClicked);
		RightSkinWidget->OnClicked_SkinFilterTypeButton.AddUniqueDynamic(
			this,
			&ThisClass::HandleSkinFilterTypeClicked);
	}
}

void UInfoSkinTabPresenter::UnbindEvents()
{
	ClearTileItemClicked();
	UInfoWidget* InfoWidget = GetInfoWidget();
	if (!InfoWidget)
	{
		return;
	}

	InfoWidget->OnDroppedSkinToCharacterPanel.RemoveDynamic(
		this,
		&ThisClass::HandleSkinDroppedToCharacter);
	if (ULeftSkinWidget* LeftSkinWidget = InfoWidget->GetLeftSkinWidget())
	{
		LeftSkinWidget->OnClicked_SkinEquipTypeSlot.RemoveDynamic(
			this,
			&ThisClass::HandleSkinEquipSlotClicked);
		LeftSkinWidget->OnDroppedSkin_SkinEquipTypeSlot.RemoveDynamic(
			this,
			&ThisClass::HandleSkinEquipSlotDropped);
	}
	if (URightSkinWidget* RightSkinWidget = InfoWidget->GetRightSkinWidget())
	{
		RightSkinWidget->OnClicked_SkinFilterAllButton.RemoveDynamic(
			this,
			&ThisClass::HandleSkinFilterAllClicked);
		RightSkinWidget->OnClicked_SkinFilterTypeButton.RemoveDynamic(
			this,
			&ThisClass::HandleSkinFilterTypeClicked);
	}
}

void UInfoSkinTabPresenter::BindTileItemClicked()
{
	UInfoWidget* InfoWidget = GetInfoWidget();
	URightSkinWidget* RightSkinWidget = InfoWidget ? InfoWidget->GetRightSkinWidget() : nullptr;
	if (UTileView* TileView = RightSkinWidget ? RightSkinWidget->GetTileView() : nullptr)
	{
		TileView->OnItemClicked().RemoveAll(this);
		TileView->OnItemClicked().AddUObject(this, &ThisClass::HandleSkinSlotClicked);
	}
}

void UInfoSkinTabPresenter::ClearTileItemClicked() const
{
	UInfoWidget* InfoWidget = GetInfoWidget();
	URightSkinWidget* RightSkinWidget = InfoWidget ? InfoWidget->GetRightSkinWidget() : nullptr;
	if (UTileView* TileView = RightSkinWidget ? RightSkinWidget->GetTileView() : nullptr)
	{
		TileView->OnItemClicked().RemoveAll(this);
	}
}

void UInfoSkinTabPresenter::ClearSkinEquipSlot(
	USkinEquipSlotWidget* TargetSkinEquipSlot,
	FGameplayTag EquipTypeTag)
{
	if (!TargetSkinEquipSlot)
	{
		return;
	}

	if (!EquipTypeTag.IsValid())
	{
		EquipTypeTag = TargetSkinEquipSlot->GetAcceptedEquipTypeTag();
	}

	TargetSkinEquipSlot->SetSkinDefinition(nullptr);
	if (SelectedEquipSlot == TargetSkinEquipSlot)
	{
		SelectedEquipSlot = nullptr;
		SelectedEquipTypeTag = FGameplayTag();
	}

	APdPlayer* Player = GetController() ? Cast<APdPlayer>(GetController()->GetPawn()) : nullptr;
	USkinEquipmentComponent* SkinEquipment = Player ? Player->GetSkinEquipmentComponent() : nullptr;
	if (!SkinEquipment || !EquipTypeTag.IsValid())
	{
		return;
	}

	const bool bRequested = SkinEquipment->RequestUnequipSkinSlot(EquipTypeTag);
	const bool bAuthority = SkinEquipment->GetOwner() && SkinEquipment->GetOwner()->HasAuthority();
	if (!bRequested || bAuthority)
	{
		RefreshEquippedSlots();
	}
}

void UInfoSkinTabPresenter::PopulateAllSkins() const
{
	if (!GetController())
	{
		return;
	}

	TArray<UObject*> CurrentSkinList;
	const APdPlayerState* PlayerState = GetPlayerState();
	const USkinComponent* SkinComponent = PlayerState ? PlayerState->GetSkinComponent() : nullptr;
	if (SkinComponent)
	{
		AppendSkinListAsObjects(SkinComponent->AllSkinList, CurrentSkinList);
	}

	UInfoWidget* InfoWidget = GetInfoWidget();
	if (URightSkinWidget* RightSkinWidget = InfoWidget ? InfoWidget->GetRightSkinWidget() : nullptr)
	{
		RightSkinWidget->SetTileView(CurrentSkinList);
	}
}
