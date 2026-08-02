#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"

class APlayerController;
class UInputAction;
struct FPdInputKeyIconSettings;

namespace PdInputKeyIconResolver
{
	UObject* ResolveMappedIconObject(const FPdInputKeyIconSettings& Settings, const FString& Candidate);
	FString GetFixedSkillSlotKeyName(int32 SkillSlotIndex);
	FString GetFixedQuickSlotKeyName(int32 QuickSlotIndex);

	UObject* ResolveIconObject(
		APlayerController* PlayerController,
		const UInputAction* InputAction,
		const FPdInputKeyIconSettings& Settings);

	FSlateBrush MakeImageBrush(UObject* ResourceObject, FVector2D ImageSize = FVector2D(64.0f, 64.0f));
	FSlateBrush MakeImageBrushFromExisting(const FSlateBrush& ExistingBrush, UObject* ResourceObject, FVector2D ImageSize);
}
