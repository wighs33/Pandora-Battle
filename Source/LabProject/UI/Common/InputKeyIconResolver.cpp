#include "UI/Common/InputKeyIconResolver.h"

#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Definition/Player/ControllerInputDefinition.h"
#include "Mode/PdPlayerController.h"

FText PdInputKeyIconResolver::ResolveInputDefinitionKeyText(APlayerController* PlayerController,
	const UInputAction* InputAction)
{
	const APdPlayerController* PdPlayerController = Cast<APdPlayerController>(PlayerController);
	const UControllerInputDefinition* InputDefinition = PdPlayerController
		? PdPlayerController->GetLoadedInputDefinition()
		: nullptr;
	return InputDefinition ? InputDefinition->ResolveInputActionKeyText(InputAction) : FText::GetEmpty();
}

UObject* PdInputKeyIconResolver::ResolveInputDefinitionIconObject(APlayerController* PlayerController,
	const UInputAction* InputAction)
{
	const APdPlayerController* PdPlayerController = Cast<APdPlayerController>(PlayerController);
	const UControllerInputDefinition* InputDefinition = PdPlayerController
		? PdPlayerController->GetLoadedInputDefinition()
		: nullptr;
	return InputDefinition ? InputDefinition->ResolveInputActionIconObject(InputAction) : nullptr;
}

FSlateBrush PdInputKeyIconResolver::MakeImageBrushFromExisting(const FSlateBrush& ExistingBrush,
	UObject* ResourceObject, const FVector2D ImageSize)
{
	FSlateBrush Brush = ExistingBrush;
	if (ImageSize.X > 0.0f && ImageSize.Y > 0.0f)
	{
		Brush.ImageSize = ImageSize;
	}
	Brush.SetResourceObject(ResourceObject);
	return Brush;
}

void PdInputKeyIconResolver::ApplyInputKeyCaption(const UUserWidget& SlotWidget, UTextBlock& KeyText,
	UWidget& InputKeyOverlay, UWidget* KeyIcon, const UInputAction* InputAction, const bool bHidden)
{
	if (KeyIcon)
	{
		KeyIcon->SetVisibility(ESlateVisibility::Collapsed);
	}
	const FText Caption = SlotWidget.IsDesignTime() ? KeyText.GetText()
		: ResolveInputDefinitionKeyText(SlotWidget.GetOwningPlayer(), InputAction);
	KeyText.SetText(Caption);
	InputKeyOverlay.SetVisibility(bHidden || Caption.IsEmpty()
		? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
}
