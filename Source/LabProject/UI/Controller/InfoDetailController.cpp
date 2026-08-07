#include "UI/Controller/InfoDetailController.h"

#include "Blueprint/SlateBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "Definition/UI/WidgetClassDefinition.h"
#include "GameFramework/PlayerController.h"
#include "Item/ItemInstance.h"
#include "Pandora/PandoraInstance.h"
#include "Skin/SkinInstance.h"
#include "UI/Widget/EquipSlotWidget.h"
#include "UI/Widget/InfoWidget.h"
#include "UI/Widget/ItemDetailWidget.h"
#include "UI/Widget/LeftEquipmentWidget.h"
#include "UI/Widget/PandoraDescriptionWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InfoDetailController)

void UInfoDetailController::Initialize(
	UInfoWidget* InOwnerWidget,
	ULeftEquipmentWidget* InEquipmentWidget,
	TSubclassOf<UItemDetailWidget> InItemDetailWidgetClass,
	TSubclassOf<UPandoraDescriptionWidget> InPandoraDescriptionWidgetClass,
	const FVector2D InPopupOffset)
{
	OwnerWidget = InOwnerWidget;
	EquipmentWidget = InEquipmentWidget;
	ItemDetailWidgetClass = InItemDetailWidgetClass;
	PandoraDescriptionWidgetClass = InPandoraDescriptionWidgetClass;
	PopupOffset = InPopupOffset;
}

void UInfoDetailController::Shutdown()
{
	HideAll();
	if (ItemDetailWidget)
	{
		ItemDetailWidget->RemoveFromParent();
	}
	if (PandoraDescriptionWidget)
	{
		PandoraDescriptionWidget->RemoveFromParent();
	}
	ItemDetailWidget = nullptr;
	PandoraDescriptionWidget = nullptr;
	EquipmentWidget = nullptr;
	OwnerWidget = nullptr;
}

void UInfoDetailController::ShowItem(
	UItemInstance* ItemInstance,
	UWidget* AnchorWidget,
	const bool bPlaceLeftOfWidget)
{
	if (!ItemInstance || !AnchorWidget)
	{
		HideAll();
		return;
	}
	UItemDetailWidget* DetailWidget = GetOrCreateItemDetailWidget();
	if (!DetailWidget)
	{
		return;
	}
	if (PandoraDescriptionWidget)
	{
		PandoraDescriptionWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	ActivePandoraAnchor.Reset();
	ActivePandoraInstance.Reset();
	DetailWidget->SetItem(ItemInstance, ResolveEquippedItemForComparison(ItemInstance));
	DetailWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PositionAdjacent(DetailWidget, AnchorWidget, bPlaceLeftOfWidget);
}

void UInfoDetailController::ShowSkin(
	USkinInstance* SkinInstance,
	UWidget* AnchorWidget,
	const bool bPlaceLeftOfWidget)
{
	if (!SkinInstance || !AnchorWidget)
	{
		HideAll();
		return;
	}
	UItemDetailWidget* DetailWidget = GetOrCreateItemDetailWidget();
	if (!DetailWidget)
	{
		return;
	}
	if (PandoraDescriptionWidget)
	{
		PandoraDescriptionWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	ActivePandoraAnchor.Reset();
	ActivePandoraInstance.Reset();
	DetailWidget->SetSkin(SkinInstance);
	DetailWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PositionAdjacent(DetailWidget, AnchorWidget, bPlaceLeftOfWidget);
}

void UInfoDetailController::ShowSkinDefinition(
	const USkinDefinition* SkinDefinition,
	UWidget* AnchorWidget,
	const bool bPlaceLeftOfWidget)
{
	if (!SkinDefinition || !AnchorWidget)
	{
		HideAll();
		return;
	}
	UItemDetailWidget* DetailWidget = GetOrCreateItemDetailWidget();
	if (!DetailWidget)
	{
		return;
	}
	if (PandoraDescriptionWidget)
	{
		PandoraDescriptionWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	ActivePandoraAnchor.Reset();
	ActivePandoraInstance.Reset();
	DetailWidget->SetSkinDefinition(SkinDefinition);
	DetailWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PositionAdjacent(DetailWidget, AnchorWidget, bPlaceLeftOfWidget);
}

void UInfoDetailController::ShowPandora(
	UPandoraInstance* PandoraInstance,
	UWidget* AnchorWidget,
	const bool bPlaceLeftOfWidget,
	const bool bPlayShowAnimation)
{
	const APlayerController* PlayerController = OwnerWidget ? OwnerWidget->GetOwningPlayer() : nullptr;
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return;
	}
	if (!PandoraInstance || !AnchorWidget)
	{
		HideAll();
		return;
	}
	UPandoraDescriptionWidget* DetailWidget = GetOrCreatePandoraDescriptionWidget();
	if (!DetailWidget)
	{
		return;
	}
	if (ActivePandoraAnchor.Get() == AnchorWidget
		&& ActivePandoraInstance.Get() == PandoraInstance
		&& DetailWidget->GetVisibility() != ESlateVisibility::Collapsed)
	{
		return;
	}
	if (ItemDetailWidget)
	{
		ItemDetailWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	ActivePandoraAnchor = AnchorWidget;
	ActivePandoraInstance = PandoraInstance;
	UPandoraDefinition* PandoraDefinition =
		const_cast<UPandoraDefinition*>(PandoraInstance->PandoraDefinition.Get());
	DetailWidget->SetPandoraDefinition(PandoraDefinition);
	DetailWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	PositionAdjacent(DetailWidget, AnchorWidget, bPlaceLeftOfWidget);
	if (bPlayShowAnimation)
	{
		DetailWidget->PlayShowAnimation();
	}
	else
	{
		DetailWidget->ShowWithoutAnimation();
	}
}

void UInfoDetailController::HideAll()
{
	if (ItemDetailWidget)
	{
		ItemDetailWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (PandoraDescriptionWidget)
	{
		PandoraDescriptionWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	ActivePandoraAnchor.Reset();
	ActivePandoraInstance.Reset();
}

void UInfoDetailController::HidePandoraForAnchor(const UWidget* AnchorWidget)
{
	if (AnchorWidget && ActivePandoraAnchor.Get() != AnchorWidget)
	{
		return;
	}
	if (PandoraDescriptionWidget)
	{
		PandoraDescriptionWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	ActivePandoraAnchor.Reset();
	ActivePandoraInstance.Reset();
}

UItemDetailWidget* UInfoDetailController::GetOrCreateItemDetailWidget()
{
	if (ItemDetailWidget)
	{
		return ItemDetailWidget;
	}
	if (!OwnerWidget || !ItemDetailWidgetClass)
	{
		return nullptr;
	}
	ItemDetailWidget = CreateWidget<UItemDetailWidget>(
		OwnerWidget->GetOwningPlayer(),
		ItemDetailWidgetClass);
	if (ItemDetailWidget)
	{
		ItemDetailWidget->AddToViewport(100);
		ItemDetailWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	return ItemDetailWidget;
}

UPandoraDescriptionWidget* UInfoDetailController::GetOrCreatePandoraDescriptionWidget()
{
	if (PandoraDescriptionWidget)
	{
		return PandoraDescriptionWidget;
	}
	if (!OwnerWidget)
	{
		return nullptr;
	}
	if (!PandoraDescriptionWidgetClass)
	{
		if (const UWidgetClassDefinition* WidgetDefinition =
			UWidgetClassDefinition::ResolveWidgetClassDefinition(OwnerWidget))
		{
			PandoraDescriptionWidgetClass = WidgetDefinition->GetPandoraDescriptionWidgetClass();
		}
	}
	if (!PandoraDescriptionWidgetClass)
	{
		return nullptr;
	}
	PandoraDescriptionWidget = CreateWidget<UPandoraDescriptionWidget>(
		OwnerWidget->GetOwningPlayer(),
		PandoraDescriptionWidgetClass);
	if (PandoraDescriptionWidget)
	{
		PandoraDescriptionWidget->AddToViewport(100);
		PandoraDescriptionWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	return PandoraDescriptionWidget;
}

void UInfoDetailController::PositionAdjacent(
	UUserWidget* DetailWidget,
	const UWidget* AnchorWidget,
	const bool bPlaceLeftOfWidget) const
{
	if (!OwnerWidget || !DetailWidget || !AnchorWidget)
	{
		return;
	}
	const FGeometry& AnchorGeometry = AnchorWidget->GetCachedGeometry();
	FVector2D PixelPosition;
	FVector2D ViewportPosition;
	USlateBlueprintLibrary::LocalToViewport(
		OwnerWidget,
		AnchorGeometry,
		bPlaceLeftOfWidget
			? FVector2D::ZeroVector
			: FVector2D(AnchorGeometry.GetLocalSize().X, 0.0f),
		PixelPosition,
		ViewportPosition);

	DetailWidget->ForceLayoutPrepass();
	const FVector2D DesiredSize = DetailWidget->GetDesiredSize();
	const FVector2D ViewportSize = UWidgetLayoutLibrary::GetViewportSize(OwnerWidget);
	FVector2D PopupPosition = bPlaceLeftOfWidget
		? FVector2D(
			ViewportPosition.X - DesiredSize.X - PopupOffset.X,
			ViewportPosition.Y + PopupOffset.Y)
		: FVector2D(
			ViewportPosition.X + PopupOffset.X,
			ViewportPosition.Y + PopupOffset.Y);
	if (ViewportSize.X > 0.0f && DesiredSize.X > 0.0f)
	{
		PopupPosition.X = FMath::Clamp(
			PopupPosition.X,
			0.0f,
			FMath::Max(ViewportSize.X - DesiredSize.X, 0.0f));
	}
	if (ViewportSize.Y > 0.0f && DesiredSize.Y > 0.0f)
	{
		PopupPosition.Y = FMath::Clamp(
			PopupPosition.Y,
			0.0f,
			FMath::Max(ViewportSize.Y - DesiredSize.Y, 0.0f));
	}
	DetailWidget->SetPositionInViewport(PopupPosition, false);
}

UItemInstance* UInfoDetailController::ResolveEquippedItemForComparison(
	UItemInstance* HoveredItem) const
{
	if (!HoveredItem || !EquipmentWidget)
	{
		return nullptr;
	}
	const UEquipSlotWidget* EquippedSlot =
		EquipmentWidget->FindFirstEquippedCompatibleEquipSlot(HoveredItem);
	UItemInstance* EquippedItem = EquippedSlot ? EquippedSlot->GetItemInstance() : nullptr;
	return EquippedItem != HoveredItem ? EquippedItem : nullptr;
}
