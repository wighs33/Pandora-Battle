#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MouseCursorWidget.generated.h"

class UImage;
class UCanvasPanel;
class USizeBox;
class UTexture2D;

UCLASS()
class LABPROJECT_API UMouseCursorWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual TSharedRef<SWidget> RebuildWidget() override;

public:
	// Public API ------------------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "!UI|Mouse Cursor")
	void ConfigureCursor(UTexture2D* InTexture, FVector2D InSize, FVector2D InHotSpot);

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	void ApplyCursorVisual();

private:
	UPROPERTY(Transient)
	TObjectPtr<USizeBox> CursorRoot;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> CursorCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UImage> CursorImage;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> CursorTexture;

	UPROPERTY(Transient)
	FVector2D CursorSize = FVector2D(32.0, 32.0);

	UPROPERTY(Transient)
	FVector2D CursorHotSpot = FVector2D::ZeroVector;
};
