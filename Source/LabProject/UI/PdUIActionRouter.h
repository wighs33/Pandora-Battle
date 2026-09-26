#pragma once

#include "CoreMinimal.h"
#include "Input/CommonUIActionRouterBase.h"
#include "PdUIActionRouter.generated.h"

/** CommonUI가 선택한 정책을 적용한다. 미전환 화면은 기본 정책만 제공한다. */
UCLASS()
class LABPROJECT_API UPdUIActionRouter : public UCommonUIActionRouterBase
{
    GENERATED_BODY()

public:
    // Engine Overrides ------------------------------------------------------------------------------------------------
    virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;
    virtual ERouteUIInputResult ProcessInput(FKey Key, EInputEvent InputEvent) const override;

    // Public API ------------------------------------------------------------------------------------------------------
    void SetFallbackInput(const FUIInputConfig& Config, TSharedPtr<SWidget> FocusWidget, bool bShowCursor);
    bool IsGameplayInputBlocked() const;

protected:
    // Internal Helpers ------------------------------------------------------------------------------------------------
    virtual void SetActiveRoot(FActivatableTreeRootPtr NewActiveRoot) override;
    virtual void ApplyUIInputConfig(const FUIInputConfig& NewConfig, bool bForceRefresh) override;
    void ApplyFallbackInput();

private:
    FUIInputConfig FallbackInput{ECommonInputMode::Game, EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown};
    TWeakPtr<SWidget> FallbackFocus;
    bool bFallbackShowCursor = false;
    mutable TSet<FKey> PressedKeys;
    mutable TSet<FKey> KeysAwaitingRelease;
};
