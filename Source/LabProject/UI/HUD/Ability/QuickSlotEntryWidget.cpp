#include "UI/HUD/Ability/QuickSlotEntryWidget.h"

#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/TextBlock.h"
#include "Definition/Player/ControllerInputDefinition.h"
#include "InputAction.h"
#include "Item/ItemInstance.h"
#include "Mode/PdPlayerController.h"
#include "Definition/Skin/SkinDefinition.h"
#include "UI/Common/InputKeyIconResolver.h"
#include "UI/Info/Item/ItemViewData.h"
#include "Engine/GameInstance.h"
#include "Localization/MenuLocalizationSubsystem.h"

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
	ApplyItemVisual();
	ApplyInputKeyIcon();
}

void UQuickSlotEntryWidget::ApplyItemVisual()
{
	const UMenuLocalizationSubsystem* Localization = UGameInstance::GetSubsystem<UMenuLocalizationSubsystem>(GetGameInstance());
	const FItemViewData ViewData = SkinDefinition
		? FItemViewDataBuilder::FromSkinDefinition(SkinDefinition, Localization)
		: FItemViewDataBuilder::FromItemInstance(ItemInstance, Localization);

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
	if (KeyText && InputKeyOverlay)
	{
		if (KeyIcon)
		{
			KeyIcon->SetVisibility(ESlateVisibility::Collapsed);
		}
		const FText Caption = IsDesignTime() ? KeyText->GetText()
			: PdInputKeyIconResolver::ResolveInputDefinitionKeyText(GetOwningPlayer(), ResolveInputAction());
		KeyText->SetText(Caption);
		InputKeyOverlay->SetVisibility(bHideInputKeyIcon || Caption.IsEmpty()
			? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
		return;
	}

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
