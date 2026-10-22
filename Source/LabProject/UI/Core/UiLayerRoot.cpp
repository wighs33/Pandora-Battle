#include "UI/Core/UiLayerRoot.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UiLayerRoot)

TSharedRef<SWidget> UUiLayerRoot::RebuildWidget()
{
	if (!MenuStack)
	{
		GameStack = WidgetTree->ConstructWidget<UCommonActivatableWidgetStack>();
		GameStack->SetTransitionDuration(0.0f);
		ScreenStack = WidgetTree->ConstructWidget<UCommonActivatableWidgetStack>();
		ScreenStack->SetTransitionDuration(0.0f);
		OverlayLayer = WidgetTree->ConstructWidget<UOverlay>();
		MenuStack = WidgetTree->ConstructWidget<UCommonActivatableWidgetStack>();
		MenuStack->SetTransitionDuration(0.0f);
		ModalStack = WidgetTree->ConstructWidget<UCommonActivatableWidgetStack>();
		ModalStack->SetTransitionDuration(0.0f);
		UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>();
		WidgetTree->RootWidget = Layers;
		// 뒤에 추가한 층이 위에 그려지고, CommonUI는 가장 위에 그려진 화면에 입력을 준다.
		for (UWidget* Layer : {static_cast<UWidget*>(GameStack), static_cast<UWidget*>(ScreenStack),
			static_cast<UWidget*>(OverlayLayer), static_cast<UWidget*>(MenuStack), static_cast<UWidget*>(ModalStack)})
		{
			UOverlaySlot* LayerSlot = Layers->AddChildToOverlay(Layer);
			LayerSlot->SetHorizontalAlignment(HAlign_Fill);
			LayerSlot->SetVerticalAlignment(VAlign_Fill);
		}
	}
	return Super::RebuildWidget();
}
