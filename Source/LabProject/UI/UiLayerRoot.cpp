#include "UI/UiLayerRoot.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UiLayerRoot)

TSharedRef<SWidget> UUiLayerRoot::RebuildWidget()
{
    if (!MenuStack)
    {
        MenuStack = WidgetTree->ConstructWidget<UCommonActivatableWidgetStack>();
        MenuStack->SetTransitionDuration(0.0f);
        ModalStack = WidgetTree->ConstructWidget<UCommonActivatableWidgetStack>();
        ModalStack->SetTransitionDuration(0.0f);
        UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>();
        WidgetTree->RootWidget = Layers;
        for (UCommonActivatableWidgetStack* Stack : {MenuStack.Get(), ModalStack.Get()})
        {
            UOverlaySlot* LayerSlot = Layers->AddChildToOverlay(Stack);
            LayerSlot->SetHorizontalAlignment(HAlign_Fill);
            LayerSlot->SetVerticalAlignment(VAlign_Fill);
        }
    }
    return Super::RebuildWidget();
}
