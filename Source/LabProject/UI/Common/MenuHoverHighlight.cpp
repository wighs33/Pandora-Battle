#include "UI/Common/MenuHoverHighlight.h"

#include "Blueprint/WidgetTree.h"
#include "CommonInputSubsystem.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MenuHoverHighlight)

DEFINE_LOG_CATEGORY_STATIC(LogMenuHoverHighlight, Log, All);

namespace
{
	/** 띠가 옅어지는 모양을 담는 텍스처의 가로 칸 수 */
	constexpr int32 BandFadeTextureWidth = 64;

	/** 화살표는 이만큼 왼쪽에서 출발해 제자리로 미끄러진다. */
	constexpr float ChevronEntryOffset = 8.f;

	/** 밑줄은 버튼 양 끝에서 이만큼 안쪽에서 시작하고 끝난다. */
	constexpr float UnderlineInset = 10.f;

	/** 밑줄과 버튼 아래 끝 사이 */
	constexpr float UnderlineGap = 2.f;

	const TCHAR* ChevronGlyph = TEXT("›");

	/** 모서리가 각진 단색 판. 텍스처 없는 Image 그리기는 아무것도 그리지 않으므로 RoundedBox를 쓴다. */
	FSlateBrush MakeFlatBrush(const FLinearColor& Color)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Color);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.CornerRadii = FVector4::Zero();
		Brush.OutlineSettings.Width = 0.f;
		return Brush;
	}

	float EaseOut(const float Progress)
	{
		return 1.f - FMath::Cube(1.f - Progress);
	}
}

void UMenuHoverHighlight::SetStyle(const FMenuHoverHighlightStyle& InStyle)
{
	Style = InStyle;
	BandFadeTexture = nullptr;
}

void UMenuHoverHighlight::Decorate(UButton* Button, const EMenuHoverMarker MarkerType)
{
	UCanvasPanel* Canvas = Button ? Cast<UCanvasPanel>(Button->GetContent()) : nullptr;
	UWidgetTree* Tree = Button ? Button->GetTypedOuter<UWidgetTree>() : nullptr;
	if (!Canvas || !Tree)
	{
		UE_LOG(LogMenuHoverHighlight, Warning, TEXT("%s has no canvas content, so it gets no hover highlight."), Button ? *Button->GetName() : TEXT("None"));
		return;
	}
	if (Entries.ContainsByPredicate([Button](const FMenuHoverEntry& Entry) { return Entry.Button == Button; }))
	{
		return;
	}

	FMenuHoverEntry& Entry = Entries.AddDefaulted_GetRef();
	Entry.Button = Button;
	Entry.MarkerType = MarkerType;
	int32 LowestZOrder = 0;
	int32 HighestZOrder = 0;
	for (int32 Index = 0; Index < Canvas->GetChildrenCount(); ++Index)
	{
		UWidget* Child = Canvas->GetChildAt(Index);
		Entry.Content.Add(Child);
		if (const UCanvasPanelSlot* ChildSlot = Cast<UCanvasPanelSlot>(Child->Slot))
		{
			LowestZOrder = FMath::Min(LowestZOrder, ChildSlot->GetZOrder());
			HighestZOrder = FMath::Max(HighestZOrder, ChildSlot->GetZOrder());
		}
	}

	// 띠: 줄 전체에 깔리고 불투명도만 바뀐다.
	Entry.Band = Tree->ConstructWidget<UImage>(UImage::StaticClass());
	FSlateBrush BandBrush = MakeFlatBrush(Style.AccentColor);
	if (MarkerType == EMenuHoverMarker::SideBar)
	{
		BandBrush.DrawAs = ESlateBrushDrawType::Image;
		BandBrush.SetResourceObject(GetBandFadeTexture());
	}
	Entry.Band->SetBrush(BandBrush);
	Entry.Band->SetVisibility(ESlateVisibility::HitTestInvisible);
	UCanvasPanelSlot* BandSlot = Canvas->AddChildToCanvas(Entry.Band);
	BandSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	BandSlot->SetOffsets(FMargin(0.f));
	BandSlot->SetZOrder(LowestZOrder - 2);

	// 표식: SideBar는 왼쪽 세로 막대가 가운데에서 위아래로, Underline은 밑줄이 왼쪽에서 오른쪽으로 자란다.
	Entry.Marker = Tree->ConstructWidget<UImage>(UImage::StaticClass());
	Entry.Marker->SetBrush(MakeFlatBrush(Style.AccentColor));
	Entry.Marker->SetVisibility(ESlateVisibility::HitTestInvisible);
	UCanvasPanelSlot* MarkerSlot = Canvas->AddChildToCanvas(Entry.Marker);
	if (MarkerType == EMenuHoverMarker::SideBar)
	{
		const float HalfLength = Style.MarkerLengthFraction * 0.5f;
		MarkerSlot->SetAnchors(FAnchors(0.f, 0.5f - HalfLength, 0.f, 0.5f + HalfLength));
		MarkerSlot->SetOffsets(FMargin(0.f, 0.f, Style.MarkerThickness, 0.f));
		Entry.Marker->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	}
	else
	{
		MarkerSlot->SetAnchors(FAnchors(0.f, 1.f, 1.f, 1.f));
		MarkerSlot->SetOffsets(FMargin(UnderlineInset, -(Style.MarkerThickness + UnderlineGap), UnderlineInset, Style.MarkerThickness));
		Entry.Marker->SetRenderTransformPivot(FVector2D(0.f, 0.5f));
	}
	MarkerSlot->SetZOrder(LowestZOrder - 1);

	// 화살표: 큰 줄의 오른쪽 끝에서 눌러 들어갈 수 있다는 것을 알린다.
	if (MarkerType == EMenuHoverMarker::SideBar)
	{
		Entry.Chevron = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Entry.Chevron->SetText(FText::AsCultureInvariant(ChevronGlyph));
		FSlateFontInfo Font = Entry.Chevron->GetFont();
		Font.Size = Style.ChevronFontSize;
		Entry.Chevron->SetFont(Font);
		Entry.Chevron->SetColorAndOpacity(FSlateColor(Style.AccentColor));
		Entry.Chevron->SetVisibility(ESlateVisibility::HitTestInvisible);
		UCanvasPanelSlot* ChevronSlot = Canvas->AddChildToCanvas(Entry.Chevron);
		ChevronSlot->SetAnchors(FAnchors(1.f, 0.5f));
		ChevronSlot->SetAlignment(FVector2D(1.f, 0.5f));
		ChevronSlot->SetPosition(FVector2D(-Style.ChevronInset, 0.f));
		ChevronSlot->SetAutoSize(true);
		ChevronSlot->SetZOrder(HighestZOrder + 1);
	}

	Apply(Entry);
}

void UMenuHoverHighlight::Tick(const float DeltaSeconds)
{
	const float Speed = 1.f / FMath::Max(Style.Duration, KINDA_SMALL_NUMBER);
	for (FMenuHoverEntry& Entry : Entries)
	{
		if (!Entry.Button)
		{
			continue;
		}
		const float Target = ShouldHighlight(*Entry.Button) ? 1.f : 0.f;
		const float Next = FMath::FInterpConstantTo(Entry.Progress, Target, DeltaSeconds, Speed);
		if (Next != Entry.Progress)
		{
			Entry.Progress = Next;
			Apply(Entry);
		}
	}
}

float UMenuHoverHighlight::GetProgress(const UWidget* Button) const
{
	const FMenuHoverEntry* Entry = Entries.FindByPredicate([Button](const FMenuHoverEntry& Candidate) { return Candidate.Button == Button; });
	return Entry ? Entry->Progress : 0.f;
}

bool UMenuHoverHighlight::ShouldHighlight(const UButton& Button) const
{
	if (!Button.GetIsEnabled())
	{
		return false;
	}
	if (Button.IsHovered())
	{
		return true;
	}
	// 마우스로 누른 버튼도 포커스를 가지므로, 포커스 강조는 패드로 고를 때만 쓴다.
	const UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(Button.GetOwningLocalPlayer());
	return Input && Input->GetCurrentInputType() == ECommonInputType::Gamepad && Button.HasAnyUserFocus();
}

void UMenuHoverHighlight::Apply(const FMenuHoverEntry& Entry) const
{
	const float Eased = EaseOut(Entry.Progress);
	Entry.Band->SetRenderOpacity(Style.BandOpacity * Eased);
	Entry.Marker->SetRenderScale(Entry.MarkerType == EMenuHoverMarker::SideBar ? FVector2D(1.f, Eased) : FVector2D(Eased, 1.f));
	if (Entry.Chevron)
	{
		Entry.Chevron->SetRenderOpacity(Eased);
		Entry.Chevron->SetRenderTranslation(FVector2D(-ChevronEntryOffset * (1.f - Eased), 0.f));
	}
	const float ContentOpacity = FMath::Lerp(Style.IdleContentOpacity, 1.f, Eased);
	for (UWidget* Content : Entry.Content)
	{
		if (Content)
		{
			Content->SetRenderTranslation(FVector2D(Style.SlideDistance * Eased, 0.f));
			Content->SetRenderOpacity(ContentOpacity);
		}
	}
}

UTexture2D* UMenuHoverHighlight::GetBandFadeTexture()
{
	if (BandFadeTexture)
	{
		return BandFadeTexture;
	}
	// 흰색에 알파만 왼쪽 1에서 BandFadeEnd까지 0으로 부드럽게 줄인다. 색은 브러시 틴트가 정한다.
	TArray<uint8> Pixels;
	Pixels.Reserve(BandFadeTextureWidth * 4);
	for (int32 X = 0; X < BandFadeTextureWidth; ++X)
	{
		const float U = (X + 0.5f) / BandFadeTextureWidth;
		const float Alpha = 1.f - FMath::SmoothStep(0.f, Style.BandFadeEnd, U);
		Pixels.Append({ 255, 255, 255, static_cast<uint8>(FMath::RoundToInt(Alpha * 255.f)) });
	}
	BandFadeTexture = UTexture2D::CreateTransient(BandFadeTextureWidth, 1, PF_B8G8R8A8, NAME_None, Pixels);
	BandFadeTexture->Filter = TF_Bilinear;
	BandFadeTexture->AddressX = TA_Clamp;
	BandFadeTexture->UpdateResource();
	return BandFadeTexture;
}
