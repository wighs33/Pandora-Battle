#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UiLayerRoot.generated.h"

class UCommonActivatableWidgetStack;
class UOverlay;

/** 화면 계층을 소유한다. Overlay 화면들은 동시에 표시할 수 있다. */
UCLASS()
class LABPROJECT_API UUiLayerRoot : public UUserWidget
{
	GENERATED_BODY()

public:
	static constexpr int32 ViewportZOrder = 1000;
	static constexpr int32 TooltipZOrder = 1100;

	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual TSharedRef<SWidget> RebuildWidget() override;

	// 게임 중 기본 화면. 위 층에 화면이 없을 때만 입력을 받는다.
	UPROPERTY(Transient)
	TObjectPtr<UCommonActivatableWidgetStack> GameStack;

	UPROPERTY(Transient)
	TObjectPtr<UCommonActivatableWidgetStack> ScreenStack;

	UPROPERTY(Transient)
	TObjectPtr<UOverlay> OverlayLayer;

	UPROPERTY(Transient)
	TObjectPtr<UCommonActivatableWidgetStack> MenuStack;

	UPROPERTY(Transient)
	TObjectPtr<UCommonActivatableWidgetStack> ModalStack;
};
