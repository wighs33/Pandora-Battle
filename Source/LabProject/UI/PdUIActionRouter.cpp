#include "UI/PdUIActionRouter.h"

#include "Component/Player/ControllerInputComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PdUIActionRouter)

void UPdUIActionRouter::PlayerControllerChanged(APlayerController* NewPlayerController)
{
    Super::PlayerControllerChanged(NewPlayerController);
    PressedKeys.Reset();
    KeysAwaitingRelease.Reset();
    FallbackFocus.Reset();
    FallbackInput = FUIInputConfig(ECommonInputMode::Game, EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown);
    bFallbackShowCursor = false;
    ActiveInputConfig.Reset();
    if (NewPlayerController) ApplyFallbackInput();
}

void UPdUIActionRouter::SetFallbackInput(const FUIInputConfig& Config, TSharedPtr<SWidget> FocusWidget, bool bShowCursor)
{
    FallbackInput = Config;
    FallbackFocus = FocusWidget;
    bFallbackShowCursor = bShowCursor;
    if (!GetActiveRoot().IsValid()) ApplyFallbackInput();
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
            if (!GetActiveRoot().IsValid()) ApplyFallbackInput();
            return false;
        }));
    }
}

void UPdUIActionRouter::ApplyFallbackInput()
{
    ApplyUIInputConfig(FallbackInput, true);
    if (TSharedPtr<SWidget> Focus = FallbackFocus.Pin())
    {
        GetLocalPlayerChecked()->GetSlateOperations().SetUserFocus(Focus.ToSharedRef());
    }
}

bool UPdUIActionRouter::IsGameplayInputBlocked() const
{
    const FUIInputConfig& Config = ActiveInputConfig.IsSet() ? ActiveInputConfig.GetValue() : FallbackInput;
    return Config.GetInputMode() == ECommonInputMode::Menu || Config.bIgnoreMoveInput || Config.bIgnoreLookInput;
}

void UPdUIActionRouter::ApplyUIInputConfig(const FUIInputConfig& NewConfig, bool bForceRefresh)
{
    const bool bWasBlocked = IsGameplayInputBlocked();
    const bool bWillBlock = NewConfig.GetInputMode() == ECommonInputMode::Menu
        || NewConfig.bIgnoreMoveInput || NewConfig.bIgnoreLookInput;
    APlayerController* Controller = GetLocalPlayerChecked()->GetPlayerController(GetWorld());
    if (bWasBlocked != bWillBlock) KeysAwaitingRelease.Append(PressedKeys);
    if (!bWillBlock && Controller)
    {
        // Slate root가 해제되면 이전 Config가 사라질 수 있다. 게임 복원 시 Held 값은 항상 비운다.
        KeysAwaitingRelease.Append(PressedKeys);
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
        const bool bShowCursor = GetActiveRoot().IsValid() ? NewConfig.GetMouseCaptureMode() == EMouseCaptureMode::NoCapture : bFallbackShowCursor;
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
