#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"

class APlayerController;
class UInputAction;

namespace PdInputKeyIconResolver
{
	FText ResolveInputDefinitionKeyText(
		APlayerController* PlayerController,
		const UInputAction* InputAction);

	UObject* ResolveInputDefinitionIconObject(
		APlayerController* PlayerController,
		const UInputAction* InputAction);

	FSlateBrush MakeImageBrushFromExisting(const FSlateBrush& ExistingBrush, UObject* ResourceObject, FVector2D ImageSize);
}
