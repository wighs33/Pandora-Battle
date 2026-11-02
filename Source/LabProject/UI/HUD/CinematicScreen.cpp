#include "UI/HUD/CinematicScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CinematicScreen)

namespace
{
	/** 검은 띠 하나의 높이(1080p 기준 UI 단위). 16:9 화면을 약 2.2:1로 보이게 한다. */
	constexpr float LetterboxBarHeight = 110.0f;

	void AddLetterboxBar(UWidgetTree& WidgetTree, UOverlay& Content, const EVerticalAlignment Alignment)
	{
		USizeBox* Bar = WidgetTree.ConstructWidget<USizeBox>();
		Bar->SetHeightOverride(LetterboxBarHeight);
		UBorder* Fill = WidgetTree.ConstructWidget<UBorder>();
		Fill->SetBrushColor(FLinearColor::Black);
		Bar->AddChild(Fill);
		UOverlaySlot* Slot = Content.AddChildToOverlay(Bar);
		Slot->SetHorizontalAlignment(HAlign_Fill);
		Slot->SetVerticalAlignment(Alignment);
	}
}

UCinematicScreen::UCinematicScreen(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bIsBackHandler = true;
}

UCinematicScreen* UCinematicScreen::Create(APlayerController* OwningPlayer)
{
	UCinematicScreen* Screen = CreateWidget<UCinematicScreen>(OwningPlayer);
	if (Screen)
	{
		UOverlay* Content = Screen->WidgetTree->ConstructWidget<UOverlay>();
		Content->SetVisibility(ESlateVisibility::HitTestInvisible);
		AddLetterboxBar(*Screen->WidgetTree, *Content, VAlign_Top);
		AddLetterboxBar(*Screen->WidgetTree, *Content, VAlign_Bottom);
		Screen->WidgetTree->RootWidget = Content;
	}
	return Screen;
}

// 메뉴 입력이라 게임 입력이 막히고, 마우스를 잡아 커서를 숨긴다.
TOptional<FUIInputConfig> UCinematicScreen::GetDesiredInputConfig() const
{
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown);
}

bool UCinematicScreen::NativeOnHandleBackAction()
{
	return true;
}
