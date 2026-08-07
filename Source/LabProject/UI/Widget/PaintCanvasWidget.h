#pragma once

#include "Blueprint/UserWidget.h"

#include "PaintCanvasWidget.generated.h"

class IWidgetCompilerLog;
class UImage;
class UWidgetTree;

/** Explicit widget contract for the paint surface embedded in the Info screen. */
UCLASS(Abstract, Blueprintable, BlueprintType)
class LABPROJECT_API UPaintCanvasWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UImage* GetCanvasImage() const { return Img_Canvas; }

protected:
	UPROPERTY(BlueprintReadOnly, Category = "!UI|Paint", meta = (BindWidget))
	TObjectPtr<UImage> Img_Canvas;

#if WITH_EDITOR
	virtual void ValidateCompiledWidgetTree(
		const UWidgetTree& BlueprintWidgetTree,
		IWidgetCompilerLog& CompileLog) const override;
#endif
};
