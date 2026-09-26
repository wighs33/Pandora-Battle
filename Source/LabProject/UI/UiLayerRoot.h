#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UiLayerRoot.generated.h"

class UCommonActivatableWidgetStack;

/** 메뉴↔가이드는 교체하고, 개인 설정 등 모달은 그 위에 겹쳐 표시한다. */
UCLASS()
class LABPROJECT_API UUiLayerRoot : public UUserWidget
{
    GENERATED_BODY()

public:
    // Engine Overrides ------------------------------------------------------------------------------------------------
    virtual TSharedRef<SWidget> RebuildWidget() override;

    UPROPERTY(Transient)
    TObjectPtr<UCommonActivatableWidgetStack> MenuStack;

    UPROPERTY(Transient)
    TObjectPtr<UCommonActivatableWidgetStack> ModalStack;
};
