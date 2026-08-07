#include "UI/Widget/InputKeyIconResolver.h"

#include "Definition/Player/ControllerInputDefinition.h"
#include "Mode/PdPlayerController.h"

UObject* PdInputKeyIconResolver::ResolveInputDefinitionIconObject(
	APlayerController* PlayerController,
	const UInputAction* InputAction)
{
	const APdPlayerController* PdPlayerController = Cast<APdPlayerController>(PlayerController);
	const UControllerInputDefinition* InputDefinition = PdPlayerController
		? PdPlayerController->GetLoadedInputDefinition()
		: nullptr;
	return InputDefinition ? InputDefinition->ResolveInputActionIconObject(InputAction) : nullptr;
}

FSlateBrush PdInputKeyIconResolver::MakeImageBrush(UObject* ResourceObject, const FVector2D ImageSize)
{
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.ImageSize = ImageSize;
	Brush.SetResourceObject(ResourceObject);
	return Brush;
}

FSlateBrush PdInputKeyIconResolver::MakeImageBrushFromExisting(
	const FSlateBrush& ExistingBrush,
	UObject* ResourceObject,
	const FVector2D ImageSize)
{
	FSlateBrush Brush = ExistingBrush;
	if (ImageSize.X > 0.0f && ImageSize.Y > 0.0f)
	{
		Brush.ImageSize = ImageSize;
	}
	Brush.SetResourceObject(ResourceObject);
	return Brush;
}
