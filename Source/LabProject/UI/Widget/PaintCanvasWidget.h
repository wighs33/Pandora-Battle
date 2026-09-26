#pragma once

#include "UI/Widget/LocalizedMenuWidget.h"

#include "PaintCanvasWidget.generated.h"

class IWidgetCompilerLog;
class UImage;
class UWidgetTree;

/** Info 화면에 포함된 페인트 영역의 위젯 구성 규약을 정의한다. */
UCLASS(Abstract, Blueprintable, BlueprintType)
class LABPROJECT_API UPaintCanvasWidget : public ULocalizedMenuWidget
{
	GENERATED_BODY()

protected:
	// Engine Overrides ------------------------------------------------------------------------------------------------
#if WITH_EDITOR
	virtual void ValidateCompiledWidgetTree(
		const UWidgetTree& BlueprintWidgetTree,
		IWidgetCompilerLog& CompileLog) const override;
#endif

public:
	// Public API ------------------------------------------------------------------------------------------------------
	UImage* GetCanvasImage() const { return Img_Canvas; }
	UImage* GetFacePreviewImage() const { return Img_FacePreview; }
	UImage* GetSpeechPreviewImage() const { return Img_SpeechPreview; }

protected:
	UPROPERTY(BlueprintReadOnly, Category = "!UI|Paint", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_FacePreview;
	UPROPERTY(BlueprintReadOnly, Category = "!UI|Paint", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_SpeechPreview;
	UPROPERTY(BlueprintReadOnly, Category = "!UI|Paint", meta = (BindWidget))
	TObjectPtr<UImage> Img_Canvas;
};
