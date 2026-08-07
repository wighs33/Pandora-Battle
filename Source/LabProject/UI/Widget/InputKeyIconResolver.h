#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"

class APlayerController;
class UInputAction;

namespace PdInputKeyIconResolver
{
	UObject* ResolveInputDefinitionIconObject(
		APlayerController* PlayerController,
		const UInputAction* InputAction);

	FSlateBrush MakeImageBrush(UObject* ResourceObject, FVector2D ImageSize = FVector2D(64.0f, 64.0f));
	FSlateBrush MakeImageBrushFromExisting(const FSlateBrush& ExistingBrush, UObject* ResourceObject, FVector2D ImageSize);
}
