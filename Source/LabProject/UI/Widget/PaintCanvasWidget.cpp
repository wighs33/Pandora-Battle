#include "UI/Widget/PaintCanvasWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"

#if WITH_EDITOR
#include "Editor/WidgetCompilerLog.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(PaintCanvasWidget)

#if WITH_EDITOR
void UPaintCanvasWidget::ValidateCompiledWidgetTree(
	const UWidgetTree& BlueprintWidgetTree,
	IWidgetCompilerLog& CompileLog) const
{
	Super::ValidateCompiledWidgetTree(BlueprintWidgetTree, CompileLog);

	const UWidget* CanvasImage = BlueprintWidgetTree.FindWidget(
		GET_MEMBER_NAME_CHECKED(ThisClass, Img_Canvas));
	if (!CanvasImage)
	{
		CompileLog.Error(NSLOCTEXT(
			"PaintCanvasWidget",
			"MissingCanvasImage",
			"Paint canvas widgets require an Image named Img_Canvas with Is Variable enabled."));
	}
	else if (!CanvasImage->IsA<UImage>())
	{
		CompileLog.Error(NSLOCTEXT(
			"PaintCanvasWidget",
			"InvalidCanvasImageType",
			"Img_Canvas must be an Image widget."));
	}
}
#endif
