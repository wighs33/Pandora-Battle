#include "UI/Core/UiScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UiScreen)

UUiScreen::UUiScreen(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    bIsBackHandler = true;
    bAutoRestoreFocus = true;
}

void UUiScreen::SetContent(UUserWidget* Panel, const FUIInputConfig& InputConfig, EPdGameplayInputPolicy GameplayPolicy,
    UWidget* FocusTarget, FSimpleDelegate BackAction)
{
    Config = InputConfig;
    GameplayInputPolicy = GameplayPolicy;
    DefaultFocus = FocusTarget;
    bAutoRestoreFocus = FocusTarget != nullptr;
    OnBack = MoveTemp(BackAction);
    UOverlay* Content = WidgetTree->ConstructWidget<UOverlay>();
    WidgetTree->RootWidget = Content;
    UOverlaySlot* ContentSlot = Content->AddChildToOverlay(Panel);
    ContentSlot->SetHorizontalAlignment(HAlign_Fill);
    ContentSlot->SetVerticalAlignment(VAlign_Fill);
}

FUIInputConfig UUiScreen::MakeBlockingInputConfig(const ECommonInputMode InputMode, const EMouseCaptureMode MouseCapture)
{
    FUIInputConfig Config(InputMode, MouseCapture);
    Config.bIgnoreMoveInput = Config.bIgnoreLookInput = true;
    return Config;
}

UUiScreen* UUiScreen::CreateBlocking(APlayerController* OwningPlayer, UUserWidget* Panel, UWidget* FocusTarget,
    FSimpleDelegate BackAction, const ECommonInputMode InputMode, const EMouseCaptureMode MouseCapture)
{
    UUiScreen* Screen = CreateWidget<UUiScreen>(OwningPlayer);
    if (Screen)
    {
        Screen->SetContent(Panel, MakeBlockingInputConfig(InputMode, MouseCapture), EPdGameplayInputPolicy::Block,
            FocusTarget, MoveTemp(BackAction));
    }
    return Screen;
}

TOptional<FUIInputConfig> UUiScreen::GetDesiredInputConfig() const
{
    return Config;
}

UWidget* UUiScreen::NativeGetDesiredFocusTarget() const
{
    return DefaultFocus.Get();
}

bool UUiScreen::NativeOnHandleBackAction()
{
    if (OnBack.IsBound()) OnBack.Execute();
    else DeactivateWidget();
    return true;
}
