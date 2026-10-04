#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"

class APlayerController;
class UInputAction;
class UTextBlock;
class UUserWidget;
class UWidget;

namespace PdInputKeyIconResolver
{
	FText ResolveInputDefinitionKeyText(
		APlayerController* PlayerController,
		const UInputAction* InputAction);

	UObject* ResolveInputDefinitionIconObject(
		APlayerController* PlayerController,
		const UInputAction* InputAction);

	FSlateBrush MakeImageBrushFromExisting(const FSlateBrush& ExistingBrush, UObject* ResourceObject, FVector2D ImageSize);

	// 키를 글자로 보여 주는 슬롯의 글자를 채우고, 글자가 없거나 숨김이면 키 묶음을 접는다. 디자이너에서는 적어 둔 글자를 둔다.
	void ApplyInputKeyCaption(
		const UUserWidget& SlotWidget,
		UTextBlock& KeyText,
		UWidget& InputKeyOverlay,
		UWidget* KeyIcon,
		const UInputAction* InputAction,
		bool bHidden);
}
