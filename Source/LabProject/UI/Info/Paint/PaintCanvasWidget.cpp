#include "UI/Info/Paint/PaintCanvasWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "UI/Common/ButtonClickRelay.h"

#if WITH_EDITOR
#include "Editor/WidgetCompilerLog.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(PaintCanvasWidget)

void UPaintCanvasWidget::NativeDestruct()
{
	UnbindPigmentButtons();
	Super::NativeDestruct();
}

#if WITH_EDITOR
void UPaintCanvasWidget::ValidateCompiledWidgetTree(const UWidgetTree& BlueprintWidgetTree,
	IWidgetCompilerLog& CompileLog) const
{
	Super::ValidateCompiledWidgetTree(BlueprintWidgetTree, CompileLog);

	const UWidget* CanvasImage = BlueprintWidgetTree.FindWidget(GET_MEMBER_NAME_CHECKED(ThisClass, Img_Canvas));
	if (!CanvasImage)
	{
		CompileLog.Error(NSLOCTEXT("PaintCanvasWidget", "MissingCanvasImage",
			"Paint canvas widgets require an Image named Img_Canvas with Is Variable enabled."));
	}
	else if (!CanvasImage->IsA<UImage>())
	{
		CompileLog.Error(NSLOCTEXT("PaintCanvasWidget", "InvalidCanvasImageType",
			"Img_Canvas must be an Image widget."));
	}
}
#endif

void UPaintCanvasWidget::SetPalette(const TArray<FColor>& InPalette)
{
	UnbindPigmentButtons();
	if (!PigmentList)
	{
		return;
	}

	for (UWidget* Child : PigmentList->GetAllChildren())
	{
		UButton* Button = Cast<UButton>(Child);
		if (!Button)
		{
			continue;
		}

		const int32 PigmentIndex = PigmentButtons.Num();
		if (!InPalette.IsValidIndex(PigmentIndex))
		{
			Button->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}

		const FColor Color = InPalette[PigmentIndex].WithAlpha(MAX_uint8);
		if (UImage* Swatch = Cast<UImage>(Button->GetContent()))
		{
			Swatch->SetColorAndOpacity(FLinearColor(Color));
		}
		Button->SetVisibility(ESlateVisibility::Visible);

		UButtonClickRelay* Relay = NewObject<UButtonClickRelay>(this);
		Relay->Bind(Button, FSimpleDelegate::CreateUObject(this, &ThisClass::SelectPigment, PigmentIndex));
		PigmentRelays.Add(Relay);
		PigmentButtons.Add(Button);
		Palette.Add(Color);
	}

	PigmentHighlight.Initialize(PigmentButtons, nullptr, SelectedPigmentBrush);
	SelectPigment(Palette.IsValidIndex(SelectedPigmentIndex) ? SelectedPigmentIndex : 0);
}

FColor UPaintCanvasWidget::GetSelectedColor() const
{
	return Palette.IsValidIndex(SelectedPigmentIndex) ? Palette[SelectedPigmentIndex] : FColor::Black;
}

void UPaintCanvasWidget::SelectPigment(const int32 PigmentIndex)
{
	if (!Palette.IsValidIndex(PigmentIndex))
	{
		return;
	}

	SelectedPigmentIndex = PigmentIndex;
	PigmentHighlight.Select(PigmentButtons[PigmentIndex]);
	PaintColorPicked.Broadcast(Palette[PigmentIndex]);
}

void UPaintCanvasWidget::UnbindPigmentButtons()
{
	for (UButtonClickRelay* Relay : PigmentRelays)
	{
		if (Relay)
		{
			Relay->Unbind();
		}
	}

	PigmentRelays.Reset();
	PigmentHighlight.Reset();
	PigmentButtons.Reset();
	Palette.Reset();
}
