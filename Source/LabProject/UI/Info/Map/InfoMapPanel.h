#pragma once

#include "CoreMinimal.h"
#include "TimerManager.h"
#include "Templates/SubclassOf.h"
#include "UObject/Object.h"
#include "InfoMapPanel.generated.h"

class UButton;
class UInfoWidget;
class UMapWidget;
class UOverlay;
class UWidgetAnimation;
class UWidgetTree;
class FContentLease;

/** 맵 콘텐츠 로딩·맵 위젯 생성·맵 오버레이 전환을 관리한다. */
UCLASS()
class LABPROJECT_API UInfoMapPanel : public UObject
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual UWorld* GetWorld() const override;

	// Public API ------------------------------------------------------------------------------------------------------
	void Initialize(
		UInfoWidget* InOwnerWidget,
		UWidgetTree* InWidgetTree,
		UButton* InMapButton,
		UOverlay* InMapOverlay,
		UMapWidget* InTotalMap,
		TSubclassOf<UMapWidget> InDefaultMapWidgetClass,
		UWidgetAnimation* InSlideAnimation,
		FVector2D InSlideStartOffset,
		float InSlideDuration);
	void BeginContentPreload();
	void Shutdown();

	bool IsDisabledForCurrentMap() const;
	bool IsOpenOrVisible() const;
	float GetHideAnimationDelay() const;
	void RefreshButtonEnabledState();
	void EnsureTotalMapWidget();
	void HideImmediately();
	void PlaySlideIn();
	void PlaySlideOut();

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	void HandleUiContentCompletion();
	void TickSlideAnimation();
	void FinishSlideOutAnimation();

	UFUNCTION()
	void HandleSlideAnimationFinished();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void BeginMapWidgetClassPreload();
	void CompleteContentPreload();
	void FailContentPreload();
	void ReleaseContentPreloads();
	void ReleaseTotalMapWidget();
	bool EnsureMapOverlay();
	TSubclassOf<UMapWidget> ResolveMapWidgetClassForCurrentMap() const;

private:
	UPROPERTY(Transient)
	TObjectPtr<UInfoWidget> OwnerWidget;
	UPROPERTY(Transient)
	TObjectPtr<UWidgetTree> WidgetTree;
	UPROPERTY(Transient)
	TObjectPtr<UButton> MapButton;
	UPROPERTY(Transient)
	TObjectPtr<UOverlay> MapOverlay;
	UPROPERTY(Transient)
	TObjectPtr<UMapWidget> TotalMap;
	UPROPERTY(Transient)
	TSubclassOf<UMapWidget> DefaultMapWidgetClass;
	UPROPERTY(Transient)
	TObjectPtr<UWidgetAnimation> SlideAnimation;

	FVector2D SlideStartOffset = FVector2D(0.0f, -96.0f);
	float SlideDuration = 0.25f;
	FTimerHandle SlideTimerHandle;
	double SlideStartTime = 0.0;
	bool bOverlayOpen = false;
	bool bSlideReverse = false;
	bool bContentReady = false;
	bool bContentPreloadRequested = false;
	bool bOpenRequested = false;
	TSharedPtr<FContentLease> MapContentLease;
	TSharedPtr<FContentLease> MapRuleLease;
	TSharedPtr<FContentLease> MapWidgetClassLease;
};
