#pragma once

#include "CoreMinimal.h"
#include "TimerManager.h"
#include "Templates/SubclassOf.h"
#include "UObject/Object.h"
#include "InfoMapController.generated.h"

class UButton;
class UInfoWidget;
class UMapWidget;
class UOverlay;
class UWidgetAnimation;
class UWidgetTree;
class FWidgetContentBundleLease;
struct FStreamableHandle;

/** Owns map content loading, map widget creation, and map-overlay transitions. */
UCLASS()
class LABPROJECT_API UInfoMapController : public UObject
{
	GENERATED_BODY()

public:
	virtual UWorld* GetWorld() const override;

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
	void BeginMapWidgetClassPreload(int32 PreloadGeneration);
	void HandleWidgetBundleCompletion(int32 PreloadGeneration);
	void CompleteContentPreload(int32 PreloadGeneration);
	void FailContentPreload(int32 PreloadGeneration);
	void ReleaseContentPreloads();
	void ReleaseTotalMapWidget();
	bool EnsureMapOverlay();
	TSubclassOf<UMapWidget> ResolveMapWidgetClassForCurrentMap() const;
	void TickSlideAnimation();
	void FinishSlideOutAnimation();

	UFUNCTION()
	void HandleSlideAnimationFinished();

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
	int32 ContentPreloadGeneration = 0;
	TSharedPtr<FWidgetContentBundleLease> MapContentBundleLease;
	TSharedPtr<FStreamableHandle> MapRulePreloadHandle;
	TSharedPtr<FStreamableHandle> MapWidgetClassPreloadHandle;
};
