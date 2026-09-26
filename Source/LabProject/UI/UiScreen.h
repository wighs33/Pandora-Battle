#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "UI/PdUIActionRouter.h"
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
    void SetContent(UUserWidget* Panel, const FUIInputConfig& InputConfig, UWidget* FocusTarget, FSimpleDelegate BackAction);

    EPdGameplayInputPolicy GameplayInputPolicy = EPdGameplayInputPolicy::Block;

private:
    FUIInputConfig Config;
    TWeakObjectPtr<UWidget> DefaultFocus;
    FSimpleDelegate OnBack;
};
