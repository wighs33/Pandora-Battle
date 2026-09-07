#include "UI/Controller/InfoMapController.h"

#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Data/ContentDataSubsystem.h"
#include "Definition/Level/LevelDefinition.h"
#include "Definition/UI/WidgetClassDefinition.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StreamableManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "Lobby/LobbyRuntimeSubsystem.h"
#include "UI/Widget/InfoWidget.h"
#include "UI/Widget/MapWidget.h"
#include "UI/UiSubsystem.h"
#include "UI/WidgetContentBundleLease.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InfoMapController)

namespace
{
float GetAnimationDuration(const UWidgetAnimation* Animation, const float FallbackDuration)
{
	return Animation
		? FMath::Max(Animation->GetEndTime() - Animation->GetStartTime(), FallbackDuration)
		: FallbackDuration;
}

TSoftObjectPtr<ULevelDefinition> GetDefaultMapUiLevelDefinition()
{
	return TSoftObjectPtr<ULevelDefinition>(
		ULevelDefinition::GetDefaultDefinitionPath());
}

const ULevelDefinition* ResolveLoadedMapUiLevelDefinition()
{
	return GetDefaultMapUiLevelDefinition().Get();
}

FString StripTravelOptions(const FString& TravelMapName)
{
	FString CleanMapName = TravelMapName;
	int32 OptionsIndex = INDEX_NONE;
	if (CleanMapName.FindChar(TEXT('?'), OptionsIndex))
	{
		CleanMapName.LeftInline(OptionsIndex, EAllowShrinking::No);
	}
	return CleanMapName;
}

bool DoesMapOptionMatchCurrentLevel(
	const FLobbyMatchMapOption& MapOption,
	const FString& CurrentPackageName,
	const FString& CurrentLevelName)
{
	const FString MapPackageName = MapOption.Map.ToSoftObjectPath().GetLongPackageName();
	if (!MapPackageName.IsEmpty()
		&& (MapPackageName.Equals(CurrentPackageName, ESearchCase::IgnoreCase)
			|| FPackageName::GetShortName(MapPackageName).Equals(
				CurrentLevelName,
				ESearchCase::IgnoreCase)))
	{
		return true;
	}

	const FString TravelMapName = StripTravelOptions(MapOption.TravelMapName);
	return !TravelMapName.IsEmpty()
		&& (TravelMapName.Equals(CurrentPackageName, ESearchCase::IgnoreCase)
			|| FPackageName::GetShortName(TravelMapName).Equals(
				CurrentLevelName,
				ESearchCase::IgnoreCase)
			|| TravelMapName.Equals(CurrentLevelName, ESearchCase::IgnoreCase));
}

bool FindMapOptionForCurrentMap(const UInfoWidget* Widget, FLobbyMatchMapOption& OutMapOption)
{
	const ULevelDefinition* Levels = ResolveLoadedMapUiLevelDefinition();
	if (!Widget || !Levels || Levels->IngameLevels.IsEmpty())
	{
		return false;
	}

	const UWorld* CurrentWorld = Widget->GetWorld();
	const FString CurrentPackageName = CurrentWorld && CurrentWorld->GetOutermost()
		? CurrentWorld->GetOutermost()->GetName()
		: FString();
	const FString CurrentLevelName = UGameplayStatics::GetCurrentLevelName(Widget, true);
	const bool bHasLevelContext = !CurrentPackageName.IsEmpty() || !CurrentLevelName.IsEmpty();
	if (CurrentWorld)
	{
		if (const ULobbyRuntimeSubsystem* LobbySubsystem = UGameInstance::GetSubsystem<ULobbyRuntimeSubsystem>(CurrentWorld->GetGameInstance()))
		{
			const FName SelectedMapKey = LobbySubsystem->GetLobbySelectedMapKey();
			if (!SelectedMapKey.IsNone()
				&& Levels->FindIngameLevel(SelectedMapKey, OutMapOption)
				&& !OutMapOption.GameplayMapWidgetClass.IsNull()
				&& (!bHasLevelContext
					|| DoesMapOptionMatchCurrentLevel(
						OutMapOption,
						CurrentPackageName,
						CurrentLevelName)))
			{
				return true;
			}
		}
	}

	for (const FLobbyMatchMapOption& MapOption : Levels->IngameLevels)
	{
		if (!MapOption.GameplayMapWidgetClass.IsNull()
			&& DoesMapOptionMatchCurrentLevel(MapOption, CurrentPackageName, CurrentLevelName))
		{
			OutMapOption = MapOption;
			return true;
		}
	}
	return false;
}
}

UWorld* UInfoMapController::GetWorld() const
{
	return OwnerWidget ? OwnerWidget->GetWorld() : Super::GetWorld();
}

void UInfoMapController::Initialize(
	UInfoWidget* InOwnerWidget,
	UWidgetTree* InWidgetTree,
	UButton* InMapButton,
	UOverlay* InMapOverlay,
	UMapWidget* InTotalMap,
	TSubclassOf<UMapWidget> InDefaultMapWidgetClass,
	UWidgetAnimation* InSlideAnimation,
	const FVector2D InSlideStartOffset,
	const float InSlideDuration)
{
	const bool bOwnerChanged = OwnerWidget != InOwnerWidget;
	OwnerWidget = InOwnerWidget;
	WidgetTree = InWidgetTree;
	MapButton = InMapButton;
	if (bOwnerChanged || InMapOverlay)
	{
		MapOverlay = InMapOverlay;
	}
	if (bOwnerChanged || InTotalMap)
	{
		TotalMap = InTotalMap;
	}
	DefaultMapWidgetClass = InDefaultMapWidgetClass;
	SlideAnimation = InSlideAnimation;
	SlideStartOffset = InSlideStartOffset;
	SlideDuration = FMath::Max(InSlideDuration, 0.01f);
}

void UInfoMapController::BeginContentPreload()
{
	if (bContentPreloadRequested)
	{
		return;
	}
	ReleaseContentPreloads();
	const int32 PreloadGeneration = ++ContentPreloadGeneration;
	bContentReady = false;
	bContentPreloadRequested = true;

	ULocalPlayer* LocalPlayer = OwnerWidget
		? OwnerWidget->GetOwningLocalPlayer()
		: nullptr;
	UUiSubsystem* UiSubsystem = LocalPlayer
		? LocalPlayer->GetSubsystem<UUiSubsystem>()
		: nullptr;
	UWidgetClassDefinition* WidgetDefinition =
		const_cast<UWidgetClassDefinition*>(
			UWidgetClassDefinition::ResolveWidgetClassDefinition(OwnerWidget));
	if (!UiSubsystem || !WidgetDefinition)
	{
		FailContentPreload(PreloadGeneration);
		return;
	}

	MapContentBundleLease = UiSubsystem->AcquireWidgetContentBundle(
		WidgetDefinition,
		EWidgetContentBundle::Map,
		FSimpleDelegate::CreateWeakLambda(
			this,
			[this, PreloadGeneration]()
			{
				HandleWidgetBundleCompletion(PreloadGeneration);
			}));
	if (!MapContentBundleLease.IsValid())
	{
		FailContentPreload(PreloadGeneration);
		return;
	}
	RefreshButtonEnabledState();
}

void UInfoMapController::HandleWidgetBundleCompletion(
	const int32 PreloadGeneration)
{
	if (PreloadGeneration != ContentPreloadGeneration)
	{
		return;
	}
	if (!MapContentBundleLease.IsValid()
		|| !MapContentBundleLease->IsReady())
	{
		FailContentPreload(PreloadGeneration);
		return;
	}

	const UGameInstance* GameInstance = OwnerWidget ? OwnerWidget->GetGameInstance() : nullptr;
	UContentDataSubsystem* ContentSubsystem = GameInstance
		? GameInstance->GetSubsystem<UContentDataSubsystem>()
		: nullptr;
	if (!ContentSubsystem)
	{
		CompleteContentPreload(PreloadGeneration);
		return;
	}

	const TSoftObjectPtr<ULevelDefinition> Levels =
		GetDefaultMapUiLevelDefinition();
	if (Levels.IsNull() || Levels.Get())
	{
		BeginMapWidgetClassPreload(PreloadGeneration);
		return;
	}
	MapRulePreloadHandle = ContentSubsystem->PreloadSoftObjectPathsAsync(
		{Levels.ToSoftObjectPath()},
		FSimpleDelegate::CreateWeakLambda(this, [this, PreloadGeneration]()
		{
			BeginMapWidgetClassPreload(PreloadGeneration);
		}));
}

void UInfoMapController::Shutdown()
{
	ReleaseContentPreloads();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SlideTimerHandle);
	}
	if (OwnerWidget && SlideAnimation)
	{
		OwnerWidget->UnbindAllFromAnimationFinished(SlideAnimation);
	}
	TotalMap = nullptr;
	MapOverlay = nullptr;
	MapButton = nullptr;
	WidgetTree = nullptr;
	OwnerWidget = nullptr;
}

bool UInfoMapController::IsDisabledForCurrentMap() const
{
	if (!OwnerWidget)
	{
		return false;
	}

	const ULevelDefinition* Levels =
		ResolveLoadedMapUiLevelDefinition();
	return Levels
		&& Levels->IsTrainingRoomMapName(
			UGameplayStatics::GetCurrentLevelName(
				OwnerWidget,
				true));
}

bool UInfoMapController::IsOpenOrVisible() const
{
	return bOverlayOpen
		|| (MapOverlay && MapOverlay->GetVisibility() != ESlateVisibility::Collapsed);
}

float UInfoMapController::GetHideAnimationDelay() const
{
	return IsOpenOrVisible() ? GetAnimationDuration(SlideAnimation, SlideDuration) : 0.0f;
}

void UInfoMapController::RefreshButtonEnabledState()
{
	if (!MapButton)
	{
		return;
	}
	const bool bDisabledForMap = IsDisabledForCurrentMap();
	const bool bDisabled = bDisabledForMap
		|| (bContentPreloadRequested && !bContentReady);
	MapButton->SetIsEnabled(!bDisabled);
	if (bDisabledForMap)
	{
		HideImmediately();
	}
}

void UInfoMapController::EnsureTotalMapWidget()
{
	if (TotalMap || !bContentReady || !OwnerWidget || !EnsureMapOverlay())
	{
		return;
	}
	TSubclassOf<UMapWidget> MapWidgetClass = ResolveMapWidgetClassForCurrentMap();
	if (!MapWidgetClass)
	{
		return;
	}
	TotalMap = CreateWidget<UMapWidget>(OwnerWidget->GetOwningPlayer(), MapWidgetClass);
	if (!TotalMap)
	{
		return;
	}
	OwnerWidget->TotalMap = TotalMap;
	TotalMap->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (UOverlaySlot* MapSlot = MapOverlay->AddChildToOverlay(TotalMap))
	{
		MapSlot->SetHorizontalAlignment(HAlign_Fill);
		MapSlot->SetVerticalAlignment(VAlign_Fill);
	}
}

void UInfoMapController::HideImmediately()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SlideTimerHandle);
	}
	bOverlayOpen = false;
	bSlideReverse = false;
	if (MapOverlay)
	{
		MapOverlay->SetVisibility(ESlateVisibility::Collapsed);
		MapOverlay->SetRenderTranslation(SlideStartOffset);
		MapOverlay->SetRenderOpacity(0.0f);
	}
	bOpenRequested = false;
	ReleaseContentPreloads();
}

void UInfoMapController::PlaySlideIn()
{
	bOpenRequested = true;
	if (!bContentReady)
	{
		BeginContentPreload();
		return;
	}
	EnsureTotalMapWidget();
	if (!MapOverlay || !OwnerWidget)
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SlideTimerHandle);
	}
	bOverlayOpen = true;
	bSlideReverse = false;
	MapOverlay->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (SlideAnimation)
	{
		OwnerWidget->StopAnimation(SlideAnimation);
		MapOverlay->SetRenderTranslation(FVector2D::ZeroVector);
		MapOverlay->SetRenderOpacity(1.0f);
		OwnerWidget->UnbindAllFromAnimationFinished(SlideAnimation);
		OwnerWidget->PlayAnimation(
			SlideAnimation,
			0.0f,
			1,
			EUMGSequencePlayMode::Forward,
			1.0f,
			false);
		return;
	}
	if (UWorld* World = GetWorld())
	{
		SlideStartTime = World->GetTimeSeconds();
		MapOverlay->SetRenderTranslation(SlideStartOffset);
		MapOverlay->SetRenderOpacity(0.0f);
		World->GetTimerManager().SetTimer(
			SlideTimerHandle,
			this,
			&ThisClass::TickSlideAnimation,
			1.0f / 60.0f,
			true);
	}
}

void UInfoMapController::PlaySlideOut()
{
	bOpenRequested = false;
	if (!MapOverlay
		|| !OwnerWidget
		|| (!bOverlayOpen && MapOverlay->GetVisibility() == ESlateVisibility::Collapsed))
	{
		ReleaseContentPreloads();
		return;
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SlideTimerHandle);
	}
	bOverlayOpen = false;
	bSlideReverse = true;
	MapOverlay->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (SlideAnimation)
	{
		OwnerWidget->StopAnimation(SlideAnimation);
		OwnerWidget->UnbindAllFromAnimationFinished(SlideAnimation);
		FWidgetAnimationDynamicEvent FinishedEvent;
		FinishedEvent.BindDynamic(this, &ThisClass::HandleSlideAnimationFinished);
		OwnerWidget->BindToAnimationFinished(SlideAnimation, FinishedEvent);
		OwnerWidget->PlayAnimationReverse(SlideAnimation, 1.0f, false);
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				SlideTimerHandle,
				this,
				&ThisClass::FinishSlideOutAnimation,
				GetAnimationDuration(SlideAnimation, SlideDuration),
				false);
		}
		return;
	}
	if (UWorld* World = GetWorld())
	{
		SlideStartTime = World->GetTimeSeconds();
		MapOverlay->SetRenderTranslation(FVector2D::ZeroVector);
		MapOverlay->SetRenderOpacity(1.0f);
		World->GetTimerManager().SetTimer(
			SlideTimerHandle,
			this,
			&ThisClass::TickSlideAnimation,
			1.0f / 60.0f,
			true);
	}
}

void UInfoMapController::BeginMapWidgetClassPreload(const int32 PreloadGeneration)
{
	if (PreloadGeneration != ContentPreloadGeneration)
	{
		return;
	}
	const UGameInstance* GameInstance = OwnerWidget ? OwnerWidget->GetGameInstance() : nullptr;
	UContentDataSubsystem* ContentSubsystem = GameInstance
		? GameInstance->GetSubsystem<UContentDataSubsystem>()
		: nullptr;
	if (!ContentSubsystem)
	{
		CompleteContentPreload(PreloadGeneration);
		return;
	}

	TArray<FSoftObjectPath> ContentPaths;
	if (const ULevelDefinition* Levels = ResolveLoadedMapUiLevelDefinition())
	{
		for (const FLobbyMatchMapOption& MapOption : Levels->IngameLevels)
		{
			if (!MapOption.GameplayMapWidgetClass.IsNull())
			{
				ContentPaths.Add(MapOption.GameplayMapWidgetClass.ToSoftObjectPath());
			}
		}
	}
	TArray<FSoftObjectPath> ExpectedContentPaths = ContentPaths;
	MapWidgetClassPreloadHandle = ContentSubsystem->PreloadSoftObjectPathsAsync(
		ContentPaths,
		FSimpleDelegate::CreateWeakLambda(
			this,
			[this,
				PreloadGeneration,
				ExpectedContentPaths = MoveTemp(ExpectedContentPaths)]()
		{
			if (PreloadGeneration != ContentPreloadGeneration)
			{
				return;
			}
			for (const FSoftObjectPath& ExpectedPath : ExpectedContentPaths)
			{
				if (ExpectedPath.IsValid() && !ExpectedPath.ResolveObject())
				{
					FailContentPreload(PreloadGeneration);
					return;
				}
			}
			CompleteContentPreload(PreloadGeneration);
		}));
}

void UInfoMapController::CompleteContentPreload(const int32 PreloadGeneration)
{
	if (PreloadGeneration != ContentPreloadGeneration)
	{
		return;
	}
	bContentReady = true;
	RefreshButtonEnabledState();
	EnsureTotalMapWidget();
	if (bOpenRequested)
	{
		PlaySlideIn();
	}
}

void UInfoMapController::FailContentPreload(const int32 PreloadGeneration)
{
	if (PreloadGeneration != ContentPreloadGeneration)
	{
		return;
	}
	bOpenRequested = false;
	ReleaseContentPreloads();
	RefreshButtonEnabledState();
}

void UInfoMapController::ReleaseContentPreloads()
{
	++ContentPreloadGeneration;
	auto ReleaseHandle = [](TSharedPtr<FStreamableHandle>& Handle)
	{
		if (Handle.IsValid())
		{
			Handle->CancelHandle();
			Handle->ReleaseHandle();
			Handle.Reset();
		}
	};
	ReleaseHandle(MapWidgetClassPreloadHandle);
	ReleaseHandle(MapRulePreloadHandle);
	MapContentBundleLease.Reset();
	bContentPreloadRequested = false;
	bContentReady = false;
	ReleaseTotalMapWidget();
}

void UInfoMapController::ReleaseTotalMapWidget()
{
	if (TotalMap)
	{
		TotalMap->RemoveFromParent();
	}
	if (OwnerWidget)
	{
		OwnerWidget->TotalMap = nullptr;
	}
	TotalMap = nullptr;
}

bool UInfoMapController::EnsureMapOverlay()
{
	if (MapOverlay)
	{
		return true;
	}
	if (!WidgetTree)
	{
		return false;
	}

	UOverlay* NewMapOverlay = WidgetTree->ConstructWidget<UOverlay>(
		UOverlay::StaticClass(),
		TEXT("MapOverlay"));
	UWidget* RootWidget = WidgetTree->RootWidget;
	if (!NewMapOverlay || !RootWidget)
	{
		return false;
	}

	if (UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(RootWidget))
	{
		if (UCanvasPanelSlot* MapOverlaySlot = RootCanvas->AddChildToCanvas(NewMapOverlay))
		{
			MapOverlaySlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			MapOverlaySlot->SetOffsets(FMargin(0.0f));
			MapOverlaySlot->SetAlignment(FVector2D::ZeroVector);
			MapOverlaySlot->SetZOrder(10);
		}
	}
	else if (UPanelWidget* RootPanel = Cast<UPanelWidget>(RootWidget))
	{
		RootPanel->AddChild(NewMapOverlay);
	}
	else
	{
		return false;
	}

	NewMapOverlay->SetVisibility(ESlateVisibility::Collapsed);
	MapOverlay = NewMapOverlay;
	if (OwnerWidget)
	{
		OwnerWidget->MapOverlay = NewMapOverlay;
	}
	return true;
}

TSubclassOf<UMapWidget> UInfoMapController::ResolveMapWidgetClassForCurrentMap() const
{
	FLobbyMatchMapOption MapOption;
	if (FindMapOptionForCurrentMap(OwnerWidget, MapOption))
	{
		UClass* LoadedWidgetClass = MapOption.GameplayMapWidgetClass.Get();
		if (LoadedWidgetClass && LoadedWidgetClass->IsChildOf(UMapWidget::StaticClass()))
		{
			return LoadedWidgetClass;
		}
		if (!MapOption.GameplayMapWidgetClass.IsNull())
		{
			return nullptr;
		}
	}

	if (DefaultMapWidgetClass)
	{
		return DefaultMapWidgetClass;
	}
	if (const UWidgetClassDefinition* WidgetDefinition =
		UWidgetClassDefinition::ResolveWidgetClassDefinition(OwnerWidget))
	{
		return WidgetDefinition->GetTotalMapWidgetClass();
	}
	return nullptr;
}

void UInfoMapController::TickSlideAnimation()
{
	UWorld* World = GetWorld();
	if (!World || !MapOverlay)
	{
		return;
	}

	const float Alpha = SlideDuration > 0.0f
		? FMath::Clamp(
			static_cast<float>((World->GetTimeSeconds() - SlideStartTime) / SlideDuration),
			0.0f,
			1.0f)
		: 1.0f;
	const float EaseAlpha = FMath::InterpEaseOut(0.0f, 1.0f, Alpha, 3.0f);
	if (bSlideReverse)
	{
		MapOverlay->SetRenderTranslation(
			FMath::Lerp(FVector2D::ZeroVector, SlideStartOffset, EaseAlpha));
		MapOverlay->SetRenderOpacity(1.0f - EaseAlpha);
	}
	else
	{
		MapOverlay->SetRenderTranslation(
			FMath::Lerp(SlideStartOffset, FVector2D::ZeroVector, EaseAlpha));
		MapOverlay->SetRenderOpacity(EaseAlpha);
	}

	if (Alpha < 1.0f)
	{
		return;
	}
	World->GetTimerManager().ClearTimer(SlideTimerHandle);
	if (bSlideReverse)
	{
		FinishSlideOutAnimation();
	}
	else
	{
		MapOverlay->SetRenderTranslation(FVector2D::ZeroVector);
		MapOverlay->SetRenderOpacity(1.0f);
	}
}

void UInfoMapController::FinishSlideOutAnimation()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SlideTimerHandle);
	}
	if (OwnerWidget && SlideAnimation)
	{
		OwnerWidget->UnbindAllFromAnimationFinished(SlideAnimation);
	}
	if (MapOverlay)
	{
		MapOverlay->SetVisibility(ESlateVisibility::Collapsed);
		MapOverlay->SetRenderTranslation(SlideStartOffset);
		MapOverlay->SetRenderOpacity(0.0f);
	}
	bOverlayOpen = false;
	bSlideReverse = false;
	ReleaseContentPreloads();
}

void UInfoMapController::HandleSlideAnimationFinished()
{
	if (bSlideReverse)
	{
		FinishSlideOutAnimation();
		return;
	}
	if (MapOverlay)
	{
		MapOverlay->SetRenderTranslation(FVector2D::ZeroVector);
		MapOverlay->SetRenderOpacity(1.0f);
	}
}
