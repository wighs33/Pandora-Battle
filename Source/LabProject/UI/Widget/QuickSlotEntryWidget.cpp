#include "UI/Widget/QuickSlotEntryWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Definition/Player/ControllerInputDefinition.h"
#include "InputAction.h"
#include "Item/ItemInstance.h"
#include "Mode/PdPlayerController.h"
#include "Definition/Skin/SkinDefinition.h"
#include "UI/Widget/InputKeyIconResolver.h"
#include "UI/Widget/ItemViewData.h"
#include "UI/WidgetLookup.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(QuickSlotEntryWidget)

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
	const FItemViewData ViewData = SkinDefinition
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

	if (bHideInputKeyIcon)
	{
		KeyIcon->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	UObject* IconObject = PdInputKeyIconResolver::ResolveInputDefinitionIconObject(
		GetOwningPlayer(),
		ResolveInputAction());

	if (!IconObject)
	{
		KeyIcon->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	KeyIcon->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	KeyIcon->SetBrush(PdInputKeyIconResolver::MakeImageBrushFromExisting(
		KeyIcon->GetBrush(),
		IconObject,
		InputKeyIconSize));
}

UInputAction* UQuickSlotEntryWidget::ResolveInputAction() const
{
	const APdPlayerController* PlayerController =
		Cast<APdPlayerController>(GetOwningPlayer());
	const UControllerInputDefinition* InputDefinition = PlayerController
		? PlayerController->GetLoadedInputDefinition()
		: nullptr;
	return InputDefinition
		? InputDefinition->GetLoadedQuickSlotInputAction(SlotIndex)
		: nullptr;
}
