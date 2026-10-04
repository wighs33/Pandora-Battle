#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "UI/Core/PdUIActionRouter.h"
#include "UiScreen.generated.h"

/** 기존 로컬라이즈된 UMG 패널의 디자인/부모를 유지하며 화면 입력과 포커스만 맡는다. */
UCLASS()
class LABPROJECT_API UUiScreen : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    // Engine Overrides ------------------------------------------------------------------------------------------------
    virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
    virtual UWidget* NativeGetDesiredFocusTarget() const override;
    virtual bool NativeOnHandleBackAction() override;

    // Public API ------------------------------------------------------------------------------------------------------
    UUiScreen(const FObjectInitializer& ObjectInitializer);
    // FocusTarget이 없으면 CommonUI가 뷰포트에 포커스를 유지하여 홀드 입력을 보존한다.
    void SetContent(UUserWidget* Panel, const FUIInputConfig& InputConfig, EPdGameplayInputPolicy GameplayPolicy,
        UWidget* FocusTarget, FSimpleDelegate BackAction);

    // 이동·시점 입력을 막는 입력 설정. 마우스는 기본으로 잡지 않는다.
    static FUIInputConfig MakeBlockingInputConfig(
        ECommonInputMode InputMode = ECommonInputMode::Menu, EMouseCaptureMode MouseCapture = EMouseCaptureMode::NoCapture);

    // Panel을 담고 이동·시점·게임플레이 입력을 막는 화면을 만든다. 층에 올리는 일은 UUiSubsystem::PushScreen이 한다.
    static UUiScreen* CreateBlocking(APlayerController* OwningPlayer, UUserWidget* Panel, UWidget* FocusTarget,
        FSimpleDelegate BackAction, ECommonInputMode InputMode = ECommonInputMode::Menu,
        EMouseCaptureMode MouseCapture = EMouseCaptureMode::NoCapture);

    EPdGameplayInputPolicy GameplayInputPolicy = EPdGameplayInputPolicy::Block;

private:
    FUIInputConfig Config;
    TWeakObjectPtr<UWidget> DefaultFocus;
    FSimpleDelegate OnBack;
};
