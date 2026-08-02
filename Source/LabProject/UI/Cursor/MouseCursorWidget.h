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

public:
	UFUNCTION(BlueprintCallable, Category = "!UI|Mouse Cursor")
	void ConfigureCursor(UTexture2D* InTexture, FVector2D InSize, FVector2D InHotSpot);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	void ApplyCursorVisual();

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
