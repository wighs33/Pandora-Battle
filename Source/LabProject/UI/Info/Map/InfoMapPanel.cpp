#include "UI/Info/Map/InfoMapPanel.h"

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
#include "UI/Core/WidgetClassDefinition.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "Lobby/LobbyRuntimeSubsystem.h"
#include "UI/Info/InfoWidget.h"
#include "UI/Info/Map/MapWidget.h"
#include "UI/Core/UiSubsystem.h"
#include "Data/ContentLease.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InfoMapPanel)

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
	return TSoftObjectPtr<ULevelDefinition>(ULevelDefinition::GetDefaultDefinitionPath());
}

const ULevelDefinition* ResolveLoadedMapUiLevelDefinition()
{
	return GetDefaultMapUiLevelDefinition().Get();
}

bool DoesMapOptionMatchCurrentLevel(const FLobbyMatchMapOption& MapOption, const FString& CurrentPackageName,
	const FString& CurrentLevelName)
{
	const FString Package = MapOption.Map.ToSoftObjectPath().GetLongPackageName();
	return !Package.IsEmpty() && (Package.Equals(CurrentPackageName, ESearchCase::IgnoreCase)
		|| FPackageName::GetShortName(Package).Equals(CurrentLevelName, ESearchCase::IgnoreCase));
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
			if (!SelectedMapKey.IsNone() && Levels->FindIngameLevel(SelectedMapKey, OutMapOption)
				&& OutMapOption.MapLayout.HasMap() && (!bHasLevelContext
				|| DoesMapOptionMatchCurrentLevel(OutMapOption, CurrentPackageName, CurrentLevelName)))
			{
				return true;
			}
		}
	}

	for (const FLobbyMatchMapOption& MapOption : Levels->IngameLevels)
	{
		if (MapOption.MapLayout.HasMap()
			&& DoesMapOptionMatchCurrentLevel(MapOption, CurrentPackageName, CurrentLevelName))
		{
			OutMapOption = MapOption;
			return true;
		}
	}
	return false;
}
}

UWorld* UInfoMapPanel::GetWorld() const
{
	return OwnerWidget ? OwnerWidget->GetWorld() : Super::GetWorld();
}

void UInfoMapPanel::Initialize(UInfoWidget* InOwnerWidget, UWidgetTree* InWidgetTree, UButton* InMapButton,
	UOverlay* InMapOverlay, UMapWidget* InTotalMap, TSubclassOf<UMapWidget> InDefaultMapWidgetClass,
	UWidgetAnimation* InSlideAnimation, const FVector2D InSlideStartOffset, const float InSlideDuration)
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

void UInfoMapPanel::BeginContentPreload()
{
	if (bContentPreloadRequested)
	{
		return;
	}
	ReleaseContentPreloads();
	bContentReady = false;
	bContentPreloadRequested = true;

	ULocalPlayer* LocalPlayer = OwnerWidget ? OwnerWidget->GetOwningLocalPlayer() : nullptr;
	UUiSubsystem* UiSubsystem = LocalPlayer ? LocalPlayer->GetSubsystem<UUiSubsystem>() : nullptr;
	const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(OwnerWidget);
	if (!UiSubsystem || !WidgetDefinition)
	{
		FailContentPreload();
		return;
	}

	MapContentLease = UiSubsystem->AcquireUiContent(WidgetDefinition, EUiContentGroup::Map,
		FSimpleDelegate::CreateUObject(this, &ThisClass::HandleUiContentCompletion));
	if (!MapContentLease.IsValid())
	{
		FailContentPreload();
		return;
	}
	RefreshButtonEnabledState();
}

void UInfoMapPanel::HandleUiContentCompletion()
{
	if (!MapContentLease.IsValid() || !MapContentLease->IsReady())
	{
		FailContentPreload();
		return;
	}

	const UGameInstance* GameInstance = OwnerWidget ? OwnerWidget->GetGameInstance() : nullptr;
	UContentDataSubsystem* ContentSubsystem = GameInstance
		? GameInstance->GetSubsystem<UContentDataSubsystem>()
		: nullptr;
	if (!ContentSubsystem)
	{
		CompleteContentPreload();
		return;
	}

	const TSoftObjectPtr<ULevelDefinition> Levels = GetDefaultMapUiLevelDefinition();
	if (Levels.IsNull() || Levels.Get())
	{
		BeginLevelMapPreload();
		return;
	}
	// 레벨 정의를 읽지 못하면 다음 단계에서 지도 그림을 찾지 못해 빈 목록으로 끝난다(지도 탭은 지도 없이 열리지 않는다).
	MapRuleLease = ContentSubsystem->AcquireContent({Levels.ToSoftObjectPath()},
		FSimpleDelegate::CreateUObject(this, &ThisClass::BeginLevelMapPreload));
}

void UInfoMapPanel::Shutdown()
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

bool UInfoMapPanel::IsDisabledForCurrentMap() const
{
	if (!OwnerWidget)
	{
		return false;
	}

	// 레벨 정의를 아직 읽지 못했으면 판단을 미루고 켜 둔다(지도는 읽은 뒤 연다).
	const ULevelDefinition* Levels = ResolveLoadedMapUiLevelDefinition();
	FLobbyMatchMapOption MapOption;
	return Levels && (Levels->IsTrainingRoomMapName(UGameplayStatics::GetCurrentLevelName(OwnerWidget, true))
		|| !FindMapOptionForCurrentMap(OwnerWidget, MapOption));
}

bool UInfoMapPanel::IsOpenOrVisible() const
{
	return bOverlayOpen || (MapOverlay && MapOverlay->GetVisibility() != ESlateVisibility::Collapsed);
}

float UInfoMapPanel::GetHideAnimationDelay() const
{
	return IsOpenOrVisible() ? GetAnimationDuration(SlideAnimation, SlideDuration) : 0.0f;
}

void UInfoMapPanel::RefreshButtonEnabledState()
{
	if (!MapButton)
	{
		return;
	}
	const bool bDisabledForMap = IsDisabledForCurrentMap();
	const bool bDisabled = bDisabledForMap || (bContentPreloadRequested && !bContentReady);
	MapButton->SetIsEnabled(!bDisabled);
	if (bDisabledForMap)
	{
		HideImmediately();
	}
}

// 지금 레벨의 지도를 지도 위젯에 넘긴다. 위젯이 없으면(디자이너가 두지 않았으면) 기본 지도 위젯을 만든다.
void UInfoMapPanel::EnsureTotalMapWidget()
{
	FLobbyMatchMapOption MapOption;
	if (!bContentReady || !OwnerWidget || !EnsureMapOverlay() || !FindMapOptionForCurrentMap(OwnerWidget, MapOption))
	{
		return;
	}
	if (!TotalMap)
	{
		const TSubclassOf<UMapWidget> MapWidgetClass = ResolveMapWidgetClass();
		TotalMap = MapWidgetClass ? CreateWidget<UMapWidget>(OwnerWidget->GetOwningPlayer(), MapWidgetClass) : nullptr;
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
	TotalMap->SetLevelMap(MapOption.MapLayout, MapOption.DisplayName);
}

void UInfoMapPanel::HideImmediately()
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

void UInfoMapPanel::PlaySlideIn()
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
		OwnerWidget->PlayAnimation(SlideAnimation, 0.0f, 1, EUMGSequencePlayMode::Forward, 1.0f, false);
		return;
	}
	if (UWorld* World = GetWorld())
	{
		SlideStartTime = World->GetTimeSeconds();
		MapOverlay->SetRenderTranslation(SlideStartOffset);
		MapOverlay->SetRenderOpacity(0.0f);
		World->GetTimerManager().SetTimer(SlideTimerHandle, this, &ThisClass::TickSlideAnimation, 1.0f / 60.0f, true);
	}
}

void UInfoMapPanel::PlaySlideOut()
{
	bOpenRequested = false;
	if (!MapOverlay || !OwnerWidget || (!bOverlayOpen && MapOverlay->GetVisibility() == ESlateVisibility::Collapsed))
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
			World->GetTimerManager().SetTimer(SlideTimerHandle, this, &ThisClass::FinishSlideOutAnimation,
				GetAnimationDuration(SlideAnimation, SlideDuration), false);
		}
		return;
	}
	if (UWorld* World = GetWorld())
	{
		SlideStartTime = World->GetTimeSeconds();
		MapOverlay->SetRenderTranslation(FVector2D::ZeroVector);
		MapOverlay->SetRenderOpacity(1.0f);
		World->GetTimerManager().SetTimer(SlideTimerHandle, this, &ThisClass::TickSlideAnimation, 1.0f / 60.0f, true);
	}
}

void UInfoMapPanel::BeginLevelMapPreload()
{
	const UGameInstance* GameInstance = OwnerWidget ? OwnerWidget->GetGameInstance() : nullptr;
	UContentDataSubsystem* ContentSubsystem = GameInstance
		? GameInstance->GetSubsystem<UContentDataSubsystem>()
		: nullptr;
	if (!ContentSubsystem)
	{
		CompleteContentPreload();
		return;
	}

	// 지금 레벨의 지도 그림(지역 지도, 전체 지도, 지역 빛)만 미리 읽는다.
	TArray<FSoftObjectPath> ContentPaths;
	FLobbyMatchMapOption MapOption;
	if (FindMapOptionForCurrentMap(OwnerWidget, MapOption))
	{
		MapOption.MapLayout.CollectAssetPaths(ContentPaths);
	}
	LevelMapLease = ContentSubsystem->AcquireContent(ContentPaths,
		FSimpleDelegate::CreateWeakLambda(this, [this]()
			{
				if (LevelMapLease.IsValid() && LevelMapLease->IsReady())
				{
					CompleteContentPreload();
				}
				else
				{
					FailContentPreload();
				}
			}));
}

void UInfoMapPanel::CompleteContentPreload()
{
	bContentReady = true;
	RefreshButtonEnabledState();
	EnsureTotalMapWidget();
	if (bOpenRequested)
	{
		PlaySlideIn();
	}
}

void UInfoMapPanel::FailContentPreload()
{
	bOpenRequested = false;
	ReleaseContentPreloads();
	RefreshButtonEnabledState();
}

void UInfoMapPanel::ReleaseContentPreloads()
{
	LevelMapLease.Reset();
	MapRuleLease.Reset();
	MapContentLease.Reset();
	bContentPreloadRequested = false;
	bContentReady = false;
	ReleaseTotalMapWidget();
}

void UInfoMapPanel::ReleaseTotalMapWidget()
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

bool UInfoMapPanel::EnsureMapOverlay()
{
	if (MapOverlay)
	{
		return true;
	}
	if (!WidgetTree)
	{
		return false;
	}

	UOverlay* NewMapOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("MapOverlay"));
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

TSubclassOf<UMapWidget> UInfoMapPanel::ResolveMapWidgetClass() const
{
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

void UInfoMapPanel::TickSlideAnimation()
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
		MapOverlay->SetRenderTranslation(FMath::Lerp(FVector2D::ZeroVector, SlideStartOffset, EaseAlpha));
		MapOverlay->SetRenderOpacity(1.0f - EaseAlpha);
	}
	else
	{
		MapOverlay->SetRenderTranslation(FMath::Lerp(SlideStartOffset, FVector2D::ZeroVector, EaseAlpha));
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

void UInfoMapPanel::FinishSlideOutAnimation()
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

void UInfoMapPanel::HandleSlideAnimationFinished()
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
