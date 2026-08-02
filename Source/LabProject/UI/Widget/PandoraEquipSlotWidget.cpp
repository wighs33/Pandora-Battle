#include "UI/Widget/PandoraEquipSlotWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "Mode/PdHUD.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "Pandora/PandoraInstance.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "UI/Widget/InfoWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PandoraEquipSlotWidget)

namespace
{
FSlateBrush MakePandoraButtonBrush(UTexture2D* IconTexture, const FLinearColor& TintColor)
{
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.TintColor = FSlateColor(TintColor);
	Brush.SetResourceObject(IconTexture);
	return Brush;
}

UInfoWidget* ResolveInfoWidgetFromPandoraEquipSlot(const UUserWidget* Widget)
{
	const APlayerController* PlayerController = Widget ? Widget->GetOwningPlayer() : nullptr;
	const APdHUD* Hud = PlayerController ? PlayerController->GetHUD<APdHUD>() : nullptr;
	return Hud ? Hud->GetInfoWidget() : nullptr;
}
}

void UPandoraEquipSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	bIsButtonHoverActive = false;
	if (ItemButton)
	{
		if (!bHasDefaultButtonStyle)
		{
			DefaultButtonStyle = ItemButton->GetStyle();
			bHasDefaultButtonStyle = true;
		}

		ItemButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleButtonClicked);
		ItemButton->OnHovered.AddUniqueDynamic(this, &ThisClass::HandleButtonHovered);
		ItemButton->OnUnhovered.AddUniqueDynamic(this, &ThisClass::HandleButtonUnhovered);
	}
}

void UPandoraEquipSlotWidget::NativeDestruct()
{
	if (bIsButtonHoverActive)
	{
		if (UInfoWidget* InfoWidget = ResolveInfoWidgetFromPandoraEquipSlot(this))
		{
			InfoWidget->HidePandoraDescriptionDetailAtWidget(this);
		}
		bIsButtonHoverActive = false;
	}

	if (ItemButton)
	{
		ItemButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleButtonClicked);
		ItemButton->OnHovered.RemoveDynamic(this, &ThisClass::HandleButtonHovered);
		ItemButton->OnUnhovered.RemoveDynamic(this, &ThisClass::HandleButtonUnhovered);
	}

	Super::NativeDestruct();
}

void UPandoraEquipSlotWidget::BroadcastClickedPandoraEquipSlot(UPandoraEquipSlotWidget* PandoraEquipSlot)
{
	OnClicked_PandoraEquipSlot.Broadcast(PandoraEquipSlot ? PandoraEquipSlot : this);
}

void UPandoraEquipSlotWidget::BroadcastHoveredPandoraEquipSlot(UPandoraEquipSlotWidget* PandoraEquipSlot)
{
	OnHovered_PandoraEquipSlot.Broadcast(PandoraEquipSlot ? PandoraEquipSlot : this);
}

void UPandoraEquipSlotWidget::SetData(UPandoraInstance* Target)
{
	CachedData = Target;
	bPandoraImageDarkened = false;
	ApplyButtonStyle();
}

void UPandoraEquipSlotWidget::SetPandoraImageDarkened(const bool bDarkened)
{
	if (bPandoraImageDarkened == bDarkened)
	{
		return;
	}

	bPandoraImageDarkened = bDarkened;
	ApplyButtonStyle();
}

void UPandoraEquipSlotWidget::ToggleText_Apply(bool bOn)
{
	if (ApplyText)
	{
		ApplyText->SetVisibility(bOn ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	}
}

void UPandoraEquipSlotWidget::HandleButtonClicked()
{
	BroadcastClickedPandoraEquipSlot(this);
}

void UPandoraEquipSlotWidget::HandleButtonHovered()
{
	if (bIsButtonHoverActive)
	{
		return;
	}

	const APlayerController* OwningPlayer = GetOwningPlayer();
	if (!OwningPlayer || !OwningPlayer->IsLocalController())
	{
		return;
	}

	bIsButtonHoverActive = true;
	BroadcastHoveredPandoraEquipSlot(this);

	if (UInfoWidget* InfoWidget = ResolveInfoWidgetFromPandoraEquipSlot(this))
	{
		if (CachedData)
		{
			InfoWidget->ShowPandoraDescriptionDetailAtWidget(CachedData, this, false);
		}
		else
		{
			InfoWidget->HideDetailWidgets();
		}
	}
}

void UPandoraEquipSlotWidget::HandleButtonUnhovered()
{
	if (!bIsButtonHoverActive)
	{
		return;
	}

	bIsButtonHoverActive = false;
	if (UInfoWidget* InfoWidget = ResolveInfoWidgetFromPandoraEquipSlot(this))
	{
		InfoWidget->HidePandoraDescriptionDetailAtWidget(this);
	}
}

void UPandoraEquipSlotWidget::ApplyButtonStyle()
{
	const UPandoraDefinition* PandoraDefinition = CachedData ? CachedData->PandoraDefinition.Get() : nullptr;
	if (!ItemButton || !PandoraDefinition)
	{
		if (ItemButton && bHasDefaultButtonStyle)
		{
			ItemButton->SetStyle(DefaultButtonStyle);
		}
		return;
	}

	UTexture2D* IconTexture = PandoraDefinition->IconTexture.Get();

	FButtonStyle ButtonStyle;
	if (bPandoraImageDarkened)
	{
		ButtonStyle.SetNormal(MakePandoraButtonBrush(IconTexture, FLinearColor(0.12f, 0.12f, 0.12f, 1.0f)));
		ButtonStyle.SetHovered(MakePandoraButtonBrush(IconTexture, FLinearColor(0.18f, 0.18f, 0.18f, 1.0f)));
		ButtonStyle.SetPressed(MakePandoraButtonBrush(IconTexture, FLinearColor(0.08f, 0.08f, 0.08f, 1.0f)));
		ButtonStyle.SetDisabled(MakePandoraButtonBrush(IconTexture, FLinearColor(0.08f, 0.08f, 0.08f, 1.0f)));
	}
	else
	{
		ButtonStyle.SetNormal(MakePandoraButtonBrush(IconTexture, FLinearColor(0.8f, 0.8f, 0.8f, 1.0f)));
		ButtonStyle.SetHovered(MakePandoraButtonBrush(IconTexture, FLinearColor(10.0f, 9.947378f, 0.0f, 0.7f)));
		ButtonStyle.SetPressed(MakePandoraButtonBrush(IconTexture, FLinearColor(1.0f, 0.954058f, 0.397999f, 0.3f)));
		ButtonStyle.SetDisabled(MakePandoraButtonBrush(IconTexture, FLinearColor(0.2f, 0.208696f, 0.573913f, 1.0f)));
	}

	ItemButton->SetStyle(ButtonStyle);
}
