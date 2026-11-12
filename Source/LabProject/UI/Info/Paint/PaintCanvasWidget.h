#pragma once

#include "UI/Common/ButtonHighlightState.h"
#include "UI/Common/LocalizedMenuWidget.h"

#include "PaintCanvasWidget.generated.h"

class IWidgetCompilerLog;
class UButton;
class UButtonClickRelay;
class UImage;
class UPanelWidget;
class UWidgetTree;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnPaintColorPicked, FColor /*Color*/);

/** Info 화면에 포함된 페인트 영역의 위젯 구성 규약을 정의한다. */
UCLASS(Abstract, Blueprintable, BlueprintType)
class LABPROJECT_API UPaintCanvasWidget : public ULocalizedMenuWidget
{
	GENERATED_BODY()

protected:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativeDestruct() override;

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

	/**
	 * 물감 칸(PigmentList 안의 버튼, 위에서부터)에 색을 하나씩 입힌다. 색보다 많은 칸은 숨긴다.
	 * 고르던 칸이 남아 있으면 그대로 두고, 없으면 첫 칸을 고른다.
	 */
	void SetPalette(const TArray<FColor>& InPalette);
	FColor GetSelectedColor() const;
	FOnPaintColorPicked& OnPaintColorPicked() { return PaintColorPicked; }

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	void SelectPigment(int32 PigmentIndex);
	void UnbindPigmentButtons();

protected:
	UPROPERTY(BlueprintReadOnly, Category = "!UI|Paint", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_FacePreview;
	UPROPERTY(BlueprintReadOnly, Category = "!UI|Paint", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_SpeechPreview;
	UPROPERTY(BlueprintReadOnly, Category = "!UI|Paint", meta = (BindWidget))
	TObjectPtr<UImage> Img_Canvas;

	/** 물감 칸 묶음. 버튼 하나가 한 색이고, 버튼 내용인 Image가 색 견본이다. */
	UPROPERTY(BlueprintReadOnly, Category = "!UI|Paint", meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> PigmentList;

	/** 고른 물감 칸의 바탕. */
	UPROPERTY(EditAnywhere, Category = "!UI|Paint")
	FSlateBrush SelectedPigmentBrush;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> PigmentButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButtonClickRelay>> PigmentRelays;

	TArray<FColor> Palette;
	int32 SelectedPigmentIndex = INDEX_NONE;
	FButtonHighlightState PigmentHighlight;
	FOnPaintColorPicked PaintColorPicked;
};
