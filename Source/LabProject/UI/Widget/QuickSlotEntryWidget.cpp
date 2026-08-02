#include "UI/Widget/QuickSlotEntryWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "InputAction.h"
#include "Item/ItemInstance.h"
#include "Definition/Skin/SkinDefinition.h"
#include "UI/Widget/InputKeyIconResolver.h"
#include "UI/Widget/ItemViewData.h"
#include "Definition/UI/WidgetClassDefinition.h"
#include "UI/WidgetLookup.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(QuickSlotEntryWidget)

namespace
{
	UObject* LoadDesignTimeQuickSlotIcon(const FQuickSlotWidgetSettings& Settings, const int32 SlotIndex)
	{
		return PdInputKeyIconResolver::ResolveMappedIconObject(
			Settings.InputKeyIconSettings,
			PdInputKeyIconResolver::GetFixedQuickSlotKeyName(SlotIndex));
	}
}

void UQuickSlotEntryWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	RefreshVisual();
}

void UQuickSlotEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	RefreshVisual();
}

void UQuickSlotEntryWidget::SetQuickSlotData(const int32 InSlotIndex, UItemInstance* InItemInstance)
{
	SlotIndex = FMath::Max(0, InSlotIndex);
	ItemInstance = InItemInstance;
	SkinDefinition = nullptr;
	RefreshVisual();
}

void UQuickSlotEntryWidget::SetGestureSlotData(const int32 InSlotIndex, const USkinDefinition* InSkinDefinition)
{
	SlotIndex = FMath::Max(0, InSlotIndex);
	ItemInstance = nullptr;
	SkinDefinition = InSkinDefinition;
	RefreshVisual();
}

void UQuickSlotEntryWidget::RefreshVisual()
{
	CacheOptionalWidgets();
	ApplyItemVisual();
	ApplyInputKeyIcon();
}

void UQuickSlotEntryWidget::CacheOptionalWidgets()
{
	if (!IconImage)
	{
		IconImage = PdWidgetLookup::FindWidgetByNames<UImage>(this, {
			TEXT("IconImage"),
			TEXT("ItemIconImage"),
			TEXT("ItemIcon"),
			TEXT("SlotIcon"),
			TEXT("Icon")
		});
	}

	if (!QuantityTextBlock)
	{
		QuantityTextBlock = PdWidgetLookup::FindWidgetByNames<UTextBlock>(this, {
			TEXT("QuantityTextBlock"),
			TEXT("Txt_Quantity"),
			TEXT("Text_Quantity"),
			TEXT("QuantityText"),
			TEXT("ItemCountText")
		});
	}

	if (!KeyIcon)
	{
		KeyIcon = PdWidgetLookup::FindWidgetByNames<UImage>(this, {
			TEXT("KeyIcon"),
			TEXT("InputKeyIcon")
		});
	}
}

void UQuickSlotEntryWidget::ApplyItemVisual()
{
	const FPdItemViewData ViewData = SkinDefinition
		? FItemViewDataBuilder::FromSkinDefinition(SkinDefinition)
		: FItemViewDataBuilder::FromItemInstance(ItemInstance);

	if (IconImage)
	{
		IconImage->SetBrushResourceObject(ViewData.IconResource);
		IconImage->SetVisibility(ViewData.IconResource ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);
	}

	if (QuantityTextBlock)
	{
		const bool bShowQuantity = ItemInstance && ViewData.HasContent() && ViewData.Quantity > 0;
		QuantityTextBlock->SetText(bShowQuantity ? FText::AsNumber(ViewData.Quantity) : FText::GetEmpty());
		QuantityTextBlock->SetVisibility(bShowQuantity ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);
	}
}

void UQuickSlotEntryWidget::ApplyInputKeyIcon()
{
	if (!KeyIcon)
	{
		return;
	}

	const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this);
	if (!WidgetDefinition)
	{
		KeyIcon->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	const FQuickSlotWidgetSettings& Settings = WidgetDefinition->GetQuickSlotWidgetSettings();
	if (Settings.InputKeyIconSettings.bHideInputKeyIcon)
	{
		KeyIcon->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	UObject* IconObject = PdInputKeyIconResolver::ResolveMappedIconObject(
		Settings.InputKeyIconSettings,
		PdInputKeyIconResolver::GetFixedQuickSlotKeyName(SlotIndex));
	if (!IconObject)
	{
		IconObject = PdInputKeyIconResolver::ResolveIconObject(
			GetOwningPlayer(),
			ResolveInputAction(),
			Settings.InputKeyIconSettings);
	}
	if (!IconObject && IsDesignTime())
	{
		IconObject = LoadDesignTimeQuickSlotIcon(Settings, SlotIndex);
	}

	if (!IconObject)
	{
		KeyIcon->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	KeyIcon->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	KeyIcon->SetBrush(PdInputKeyIconResolver::MakeImageBrushFromExisting(
		KeyIcon->GetBrush(),
		IconObject,
		Settings.InputKeyIconSettings.IconSize));
}

UInputAction* UQuickSlotEntryWidget::ResolveInputAction() const
{
	const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this);
	if (!WidgetDefinition)
	{
		return nullptr;
	}

	const FQuickSlotWidgetSettings& Settings = WidgetDefinition->GetQuickSlotWidgetSettings();
	const TSoftObjectPtr<UInputAction>* InputAction = nullptr;
	switch (SlotIndex)
	{
	case 0:
		InputAction = &Settings.QuickSlot1InputAction;
		break;
	case 1:
		InputAction = &Settings.QuickSlot2InputAction;
		break;
	case 2:
		InputAction = &Settings.QuickSlot3InputAction;
		break;
	case 3:
		InputAction = &Settings.QuickSlot4InputAction;
		break;
	case 4:
		InputAction = &Settings.GestureSlot1InputAction;
		break;
	case 5:
		InputAction = &Settings.GestureSlot2InputAction;
		break;
	case 6:
		InputAction = &Settings.GestureSlot3InputAction;
		break;
	case 7:
		InputAction = &Settings.GestureSlot4InputAction;
		break;
	default:
		break;
	}

	return InputAction ? InputAction->Get() : nullptr;
}
