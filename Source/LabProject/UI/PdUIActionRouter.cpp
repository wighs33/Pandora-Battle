#include "UI/PdUIActionRouter.h"

#include "Component/Player/ControllerInputComponent.h"
#include "UI/UiScreen.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PdUIActionRouter)

void UPdUIActionRouter::PlayerControllerChanged(APlayerController* NewPlayerController)
{
    Super::PlayerControllerChanged(NewPlayerController);
    PressedKeys.Reset();
    KeysAwaitingRelease.Reset();
    ChatInputWidget.Reset();
    AppliedGameplayPolicy = EPdGameplayInputPolicy::Allow;
    ActiveInputConfig.Reset();
    if (NewPlayerController) ApplyDefaultInput();
}

void UPdUIActionRouter::BeginChatInput(UWidget* InputWidget)
{
    ChatInputWidget = InputWidget;
    if (!GetActiveRoot().IsValid()) ApplyDefaultInput();
}

void UPdUIActionRouter::EndChatInput(const UWidget* InputWidget)
{
    if (ChatInputWidget.Get() != InputWidget) return;
    ChatInputWidget.Reset();
    if (!GetActiveRoot().IsValid()) ApplyDefaultInput();
}

void UPdUIActionRouter::SetActiveRoot(FActivatableTreeRootPtr NewActiveRoot)
{
    Super::SetActiveRoot(NewActiveRoot);
    if (!GetActiveRoot().IsValid())
    {
        // UE 5.8은 Slate root 제거를 마친 뒤 ActiveInputConfig를 비운다.
        // 그 정리가 끝난 다음 기본 정책을 복원한다. Core ticker는 일시정지 중에도 실행된다.
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float)
        {
            if (!GetActiveRoot().IsValid()) ApplyDefaultInput();
            return false;
        }));
    }
}

void UPdUIActionRouter::ApplyDefaultInput()
{
    UWidget* ChatInput = ChatInputWidget.Get();
    FUIInputConfig Config(ChatInput ? ECommonInputMode::All : ECommonInputMode::Game,
        EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown);
    Config.bIgnoreMoveInput = Config.bIgnoreLookInput = ChatInput != nullptr;
    ApplyUIInputConfig(Config, true);
    if (ChatInput) GetLocalPlayerChecked()->GetSlateOperations().SetUserFocus(ChatInput->TakeWidget());
}

bool UPdUIActionRouter::IsGameplayInputBlocked() const
{
    return AppliedGameplayPolicy == EPdGameplayInputPolicy::Block;
}

void UPdUIActionRouter::ApplyUIInputConfig(const FUIInputConfig& NewConfig, bool bForceRefresh)
{
    const bool bWasBlocked = IsGameplayInputBlocked();
    const UCommonActivatableWidget* ActiveScreen = GetLeafmostActivatableWidget();
    const UUiScreen* Adapter = Cast<UUiScreen>(ActiveScreen);
    // 직접 전환한 메뉴는 Block, 기존 위젯 어댑터는 선언된 게임플레이 정책을 사용한다.
    AppliedGameplayPolicy = Adapter ? Adapter->GameplayInputPolicy
        : ActiveScreen || ChatInputWidget.IsValid() ? EPdGameplayInputPolicy::Block : EPdGameplayInputPolicy::Allow;
    const bool bWillBlock = IsGameplayInputBlocked();
    APlayerController* Controller = GetLocalPlayerChecked()->GetPlayerController(GetWorld());
    if (bWasBlocked != bWillBlock) KeysAwaitingRelease.Append(PressedKeys);
    if (bWasBlocked && !bWillBlock && Controller)
    {
        // 차단 해제 때만 정리한다. Allow 정책 재적용은 누른 입력을 유지한다.
        Controller->FlushPressedKeys();
    }
    if (!bWasBlocked && bWillBlock && Controller)
    {
        if (UControllerInputComponent* Input = Controller->FindComponentByClass<UControllerInputComponent>())
        {
            Input->ReleaseGameplayInput();
        }
    }
    if (NewConfig.GetInputMode() == ECommonInputMode::Menu && GetActiveInputMode() != ECommonInputMode::Menu && Controller)
    {
        if (UControllerInputComponent* Input = Controller->FindComponentByClass<UControllerInputComponent>())
            Input->ReleaseHeldUiInput();
    }
    Super::ApplyUIInputConfig(NewConfig, bForceRefresh);
    if (Controller)
    {
        // CommonUI가 입력을 분배하므로 이전 UIOnly의 전역 Viewport 차단은 사용하지 않는다.
        if (UGameViewportClient* Viewport = GetLocalPlayerChecked()->ViewportClient) Viewport->SetIgnoreInput(false);
        const bool bShowCursor = NewConfig.GetMouseCaptureMode() == EMouseCaptureMode::NoCapture;
        Controller->SetShowMouseCursor(bShowCursor);
        Controller->bEnableClickEvents = bShowCursor;
        Controller->bEnableMouseOverEvents = bShowCursor;
    }
}

ERouteUIInputResult UPdUIActionRouter::ProcessInput(FKey Key, EInputEvent InputEvent) const
{
    if (InputEvent == IE_Pressed) PressedKeys.Add(Key);
    if (InputEvent == IE_Released)
    {
        PressedKeys.Remove(Key);
        KeysAwaitingRelease.Remove(Key);
    }
    const ERouteUIInputResult Result = Super::ProcessInput(Key, InputEvent);
    // UI를 여닫기 전에 누른 키는 실제 Release 전까지 게임으로 다시 전달하지 않는다.
    return Result == ERouteUIInputResult::Unhandled && KeysAwaitingRelease.Contains(Key)
        ? ERouteUIInputResult::BlockGameInput : Result;
}
