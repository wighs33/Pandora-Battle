#include "UI/Info/Map/MapWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "EngineUtils.h"
#include "Engine/Texture2D.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "Mode/PdPlayerState.h"
#include "TimerManager.h"
#include "UI/Common/ButtonClickRelay.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MapWidget)

namespace
{
	/** 전체 지도에서 고를 수 있는 지역(지역 단추가 있는 지역). */
	constexpr EPlayerMapRegion SelectableRegions[] = {EPlayerMapRegion::Windmill, EPlayerMapRegion::Dome, EPlayerMapRegion::Temple};

	/** 지역 빛은 지도 그림 위, 지역 단추와 표식 아래에 그린다. */
	constexpr int32 RegionHighlightZOrder = 5;
	constexpr int32 RegionButtonZOrder = 20;

	/** 지역별 표식 줄은 지역 단추 아래에 둔다(단추 가운데에서 표식 줄 위까지, 화면 px). */
	constexpr float RegionMarkerRowOffsetY = 34.0f;

	void PlaceCentered(UWidget* Widget, const FVector2D& Position, const FVector2D& Alignment, const int32 ZOrder)
	{
		if (UCanvasPanelSlot* CanvasSlot = Widget ? Cast<UCanvasPanelSlot>(Widget->Slot) : nullptr)
		{
			CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f));
			CanvasSlot->SetAlignment(Alignment);
			CanvasSlot->SetPosition(Position);
			CanvasSlot->SetZOrder(ZOrder);
		}
	}
}

UMapWidget::UMapWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMapWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ApplyWidgetDefinitionSettings();
	HideDesignerMarkerWidgets();
	BindRegionButtons();
	if (AreaMapButton)
	{
		AreaMapButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnAreaMapButtonClicked);
	}

	ApplyLevelMap();
	StartMapUpdateTimers();
}

void UMapWidget::NativeDestruct()
{
	StopMapUpdateTimers();
	UnbindRegionButtons();
	if (AreaMapButton)
	{
		AreaMapButton->OnClicked.RemoveDynamic(this, &ThisClass::OnAreaMapButtonClicked);
	}

	PawnMarkers.RemoveAll();
	CachedRemotePlayerPawns.Reset();
	AreaRegionMarkers.Reset();

	Super::NativeDestruct();
}

void UMapWidget::SetLevelMap(const FLevelMapLayout& InLayout, const FText& InLevelName)
{
	Layout = InLayout;
	LevelName = InLevelName;
	bHasSyncedPlayerPage = false;
	bHasManualPageSelection = false;
	if (IsConstructed())
	{
		ApplyLevelMap();
	}
}

void UMapWidget::ShowAreaMap()
{
	bHasManualPageSelection = true;
	ShowPage(INDEX_NONE);
}

void UMapWidget::ShowRegionMap(const EPlayerMapRegion Region)
{
	const int32 PageIndex = Layout.FindPageIndex(Region);
	if (PageIndex == INDEX_NONE)
	{
		return;
	}
	bHasManualPageSelection = true;
	ShowPage(PageIndex);
}

bool UMapWidget::IsRegionHighlighted(const EPlayerMapRegion Region) const
{
	const int32 PageIndex = Layout.FindPageIndex(Region);
	const UImage* Highlight = RegionHighlights.IsValidIndex(PageIndex) ? RegionHighlights[PageIndex].Get() : nullptr;
	return Highlight && Highlight->IsVisible();
}

void UMapWidget::OnAreaMapButtonClicked()
{
	ShowAreaMap();
}

void UMapWidget::HandleRegionHoverChanged(const bool bHovered, const EPlayerMapRegion Region)
{
	SetRegionHighlightVisible(Layout.FindPageIndex(Region), bHovered && ShownPageIndex == INDEX_NONE);
}

// 레벨 지도가 바뀌면 전체 지도의 지역 단추·빛을 다시 놓고, 플레이어가 있는 지도(없으면 전체 지도)를 보인다.
void UMapWidget::ApplyLevelMap()
{
	PlaceAreaRegionWidgets();
	SyncShownPageToPlayerRegion();
	if (!bHasSyncedPlayerPage)
	{
		ShowPage(INDEX_NONE);
	}
	RefreshRemotePlayerPawns();
	HandleMapUpdateTick();
}

// 전체 지도에서는 지역 단추와 지역별 표식 줄을, 지역 지도에서는 그 지역의 플레이어 표식을 보인다.
// 전체 지도가 없는 레벨(지도가 한 장)에서 INDEX_NONE을 받으면 그 한 장을 보인다.
void UMapWidget::ShowPage(const int32 PageIndex)
{
	const bool bAreaView = PageIndex == INDEX_NONE && Layout.HasAreaView();
	ShownPageIndex = bAreaView ? INDEX_NONE : (Layout.Pages.IsValidIndex(PageIndex) ? PageIndex : 0);
	if (!bAreaView && !Layout.Pages.IsValidIndex(ShownPageIndex))
	{
		ShownPageIndex = INDEX_NONE;
		return;
	}

	UTexture2D* Texture = bAreaView ? Layout.AreaView.Texture.Get() : Layout.Pages[ShownPageIndex].Texture.Get();
	if (MapImage && Texture)
	{
		MapImage->SetBrushFromTexture(Texture);
	}
	if (TotalSizeBox)
	{
		const FVector2D ViewSize = GetShownViewSize();
		TotalSizeBox->SetWidthOverride(ViewSize.X);
		TotalSizeBox->SetHeightOverride(ViewSize.Y);
	}
	if (MapName)
	{
		MapName->SetText(GetShownMapName());
	}
	SetRegionSelectionVisible(bAreaView);
	if (AreaMapButton)
	{
		AreaMapButton->SetVisibility(Layout.HasAreaView() && !bAreaView ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	for (int32 HighlightIndex = 0; HighlightIndex < RegionHighlights.Num(); ++HighlightIndex)
	{
		SetRegionHighlightVisible(HighlightIndex, false);
	}

	if (bAreaView)
	{
		PawnMarkers.HideSelfMarks();
		PawnMarkers.HideRemoteMarks();
		RefreshAreaMapRegionMarkers();
		return;
	}

	UpdateCharacterMark();
	GetAreaRegionBoxes().SetVisible(false);
	RefreshRemotePlayerPawns();
	UpdateTeamMarks();
}

void UMapWidget::BindRegionButtons()
{
	UnbindRegionButtons();
	for (const EPlayerMapRegion Region : SelectableRegions)
	{
		if (UButton* Button = GetRegionButton(Region))
		{
			UButtonClickRelay* Relay = NewObject<UButtonClickRelay>(this);
			Relay->Bind(Button, FSimpleDelegate::CreateUObject(this, &ThisClass::ShowRegionMap, Region),
				UButtonClickRelay::FOnHoverChanged::CreateUObject(this, &ThisClass::HandleRegionHoverChanged, Region));
			RegionButtonRelays.Add(Relay);
		}
	}
}

void UMapWidget::UnbindRegionButtons()
{
	for (UButtonClickRelay* Relay : RegionButtonRelays)
	{
		if (Relay)
		{
			Relay->Unbind();
		}
	}
	RegionButtonRelays.Reset();
}

// 지역 단추를 지역 가운데(AreaAnchor)를 전체 지도 카메라로 비춘 자리에 놓고, 지역별 표식 줄을 그 아래에 둔다.
// 지역 빛은 전체 지도와 같은 크기의 그림이라 지도 그림을 덮도록 깐다.
void UMapWidget::PlaceAreaRegionWidgets()
{
	const FVector2D AreaViewSize(FMath::Min(MaxMapViewSize.X, MaxMapViewSize.Y));
	const FMapAreaRegionBoxes RegionBoxes = GetAreaRegionBoxes();
	for (const EPlayerMapRegion Region : SelectableRegions)
	{
		const int32 PageIndex = Layout.FindPageIndex(Region);
		if (PageIndex == INDEX_NONE || !Layout.HasAreaView())
		{
			continue;
		}
		const FVector2D Position = Layout.AreaView.ToMapUV(Layout.Pages[PageIndex].AreaAnchor) * AreaViewSize;
		PlaceCentered(GetRegionButton(Region), Position, FVector2D(0.5f, 0.5f), RegionButtonZOrder);
		PlaceCentered(RegionBoxes.Resolve(Region), Position + FVector2D(0.0f, RegionMarkerRowOffsetY), FVector2D(0.5f, 0.0f),
			RegionButtonZOrder);
	}

	RegionHighlights.SetNum(Layout.Pages.Num());
	UPanelWidget* Canvas = GetMarkerParentPanel();
	for (int32 PageIndex = 0; PageIndex < Layout.Pages.Num(); ++PageIndex)
	{
		UTexture2D* HighlightTexture = Layout.HasAreaView() ? Layout.Pages[PageIndex].AreaHighlight.Get() : nullptr;
		if (!HighlightTexture || !Canvas || !WidgetTree)
		{
			continue;
		}
		UImage* Highlight = RegionHighlights[PageIndex];
		if (!Highlight)
		{
			Highlight = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			RegionHighlights[PageIndex] = Highlight;
			if (UCanvasPanelSlot* HighlightSlot = Cast<UCanvasPanelSlot>(Canvas->AddChild(Highlight)))
			{
				HighlightSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
				HighlightSlot->SetOffsets(FMargin(0.0f));
				HighlightSlot->SetZOrder(RegionHighlightZOrder);
			}
		}
		Highlight->SetBrushFromTexture(HighlightTexture);
		Highlight->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UMapWidget::SetRegionHighlightVisible(const int32 PageIndex, const bool bVisible)
{
	if (UImage* Highlight = RegionHighlights.IsValidIndex(PageIndex) ? RegionHighlights[PageIndex].Get() : nullptr)
	{
		Highlight->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

UButton* UMapWidget::GetRegionButton(const EPlayerMapRegion Region) const
{
	switch (Region)
	{
	case EPlayerMapRegion::Windmill:
		return WindmillButton;
	case EPlayerMapRegion::Dome:
		return DomeButton;
	case EPlayerMapRegion::Temple:
		return TempleButton;
	default:
		return nullptr;
	}
}

void UMapWidget::SetRegionSelectionVisible(const bool bVisible)
{
	for (const EPlayerMapRegion Region : SelectableRegions)
	{
		if (UButton* Button = GetRegionButton(Region))
		{
			const bool bHasRegion = Layout.HasAreaView() && Layout.FindPageIndex(Region) != INDEX_NONE;
			Button->SetVisibility(bVisible && bHasRegion ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}
	}
}

void UMapWidget::ApplyWidgetDefinitionSettings()
{
	// 정의 데이터에 팀 표식 그림이 없을 때 디자이너가 둔 팀 표식 그림을 대신 쓴다.
	MarkImages.SetDesignerTeamMark(TeamMark);

	const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this);
	if (!WidgetDefinition)
	{
		return;
	}

	const FMapWidgetSettings& Settings = WidgetDefinition->GetMapWidgetSettings();
	bShowRemotePlayerMarks = Settings.bShowRemotePlayerMarks;
	MarkerUpdateInterval = FMath::Max(Settings.MarkerUpdateInterval, 0.02f);
	RemotePlayerListRefreshInterval = FMath::Max(Settings.RemotePlayerListRefreshInterval, 0.1f);
	bRotateMarksToPawnForward = Settings.bRotateMarksToPawnForward;
	MarkerRotationOffsetDegrees = Settings.MarkerRotationOffsetDegrees;
	MaxMapViewSize = FVector2D(FMath::Max(Settings.MaxMapViewSize.X, 1.0f), FMath::Max(Settings.MaxMapViewSize.Y, 1.0f));
	MarkImages.Configure(Settings);
}

// 플레이어가 다른 지역으로 옮기면 그 지역 지도로 따라간다. 직접 고른 지도는 플레이어 지역이 바뀔 때까지 둔다.
void UMapWidget::SyncShownPageToPlayerRegion()
{
	const APawn* OwningPawn = GetOwningPlayerPawn();
	const int32 PlayerPageIndex = OwningPawn ? FindPlayerPageIndex(*OwningPawn) : INDEX_NONE;
	if (PlayerPageIndex == INDEX_NONE)
	{
		return;
	}

	if (bHasManualPageSelection)
	{
		if (bHasSyncedPlayerPage && LastSyncedPlayerPageIndex == PlayerPageIndex)
		{
			return;
		}
		bHasManualPageSelection = false;
	}

	const bool bIsShowingAllowedPage = ShownPageIndex == PlayerPageIndex || ShownPageIndex == INDEX_NONE;
	if (bHasSyncedPlayerPage && LastSyncedPlayerPageIndex == PlayerPageIndex && bIsShowingAllowedPage)
	{
		return;
	}

	bHasSyncedPlayerPage = true;
	LastSyncedPlayerPageIndex = PlayerPageIndex;
	ShowPage(PlayerPageIndex);
}

void UMapWidget::StartMapUpdateTimers()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().SetTimer(MarkerUpdateTimerHandle, this, &ThisClass::HandleMapUpdateTick,
		FMath::Max(MarkerUpdateInterval, 0.02f), true);

	World->GetTimerManager().SetTimer(RemotePlayerListRefreshTimerHandle, this, &ThisClass::RefreshRemotePlayerPawns,
		FMath::Max(RemotePlayerListRefreshInterval, 0.1f), true);
}

void UMapWidget::StopMapUpdateTimers()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MarkerUpdateTimerHandle);
		World->GetTimerManager().ClearTimer(RemotePlayerListRefreshTimerHandle);
	}

	MarkerUpdateTimerHandle.Invalidate();
	RemotePlayerListRefreshTimerHandle.Invalidate();
}

void UMapWidget::HandleMapUpdateTick()
{
	SyncShownPageToPlayerRegion();
	UpdateCharacterMark();
	UpdateTeamMarks();
}

void UMapWidget::RefreshRemotePlayerPawns()
{
	CachedRemotePlayerPawns.Reset();
	RefreshAreaMapRegionMarkers();

	if (!bShowRemotePlayerMarks)
	{
		return;
	}

	UWorld* World = GetWorld();
	APawn* OwningPawn = GetOwningPlayerPawn();
	const APdPlayerState* OwningPlayerState = OwningPawn ? OwningPawn->GetPlayerState<APdPlayerState>() : nullptr;
	if (!World || !OwningPawn || !OwningPlayerState)
	{
		return;
	}

	for (TActorIterator<APawn> It(World); It; ++It)
	{
		APawn* CandidatePawn = *It;
		if (!CandidatePawn || CandidatePawn == OwningPawn)
		{
			continue;
		}

		const APdPlayerState* CandidatePlayerState = CandidatePawn->GetPlayerState<APdPlayerState>();
		if (!CandidatePlayerState || CandidatePlayerState == OwningPlayerState)
		{
			continue;
		}

		CachedRemotePlayerPawns.Add(CandidatePawn);
	}
}

void UMapWidget::RefreshAreaMapRegionMarkers()
{
	const FMapAreaRegionBoxes RegionBoxes = GetAreaRegionBoxes();
	const bool bShowAreaMarkers = ShownPageIndex == INDEX_NONE && Layout.HasAreaView();
	RegionBoxes.SetVisible(bShowAreaMarkers);
	if (!bShowAreaMarkers)
	{
		return;
	}

	const UWorld* World = GetWorld();
	AreaRegionMarkers.Refresh(
		*this,
		RegionBoxes,
		World ? World->GetGameState() : nullptr,
		MarkImages);
}

void UMapWidget::UpdateCharacterMark()
{
	const APawn* OwningPawn = GetOwningPlayerPawn();
	if (ShownPageIndex == INDEX_NONE || !OwningPawn)
	{
		PawnMarkers.HideSelfMarks();
		return;
	}

	PawnMarkers.UpdateSelfMarks(MakeMarkerCanvas(), *OwningPawn, DoesPawnMatchShownPage(*OwningPawn));
}

void UMapWidget::UpdateTeamMarks()
{
	if (!bShowRemotePlayerMarks || ShownPageIndex == INDEX_NONE)
	{
		PawnMarkers.HideRemoteMarks();
		return;
	}

	APawn* OwningPawn = GetOwningPlayerPawn();
	const APdPlayerState* OwningPlayerState = OwningPawn ? OwningPawn->GetPlayerState<APdPlayerState>() : nullptr;
	if (!OwningPawn || !OwningPlayerState)
	{
		PawnMarkers.HideRemoteMarks();
		return;
	}

	TArray<const APawn*> RemotePawnsOnShownPage;
	RemotePawnsOnShownPage.Reserve(CachedRemotePlayerPawns.Num());
	for (int32 PawnIndex = CachedRemotePlayerPawns.Num() - 1; PawnIndex >= 0; --PawnIndex)
	{
		const APawn* CandidatePawn = CachedRemotePlayerPawns[PawnIndex].Get();
		if (!CandidatePawn || CandidatePawn == OwningPawn)
		{
			CachedRemotePlayerPawns.RemoveAtSwap(PawnIndex);
			continue;
		}

		const APdPlayerState* CandidatePlayerState = CandidatePawn->GetPlayerState<APdPlayerState>();
		if (!CandidatePlayerState || CandidatePlayerState == OwningPlayerState)
		{
			CachedRemotePlayerPawns.RemoveAtSwap(PawnIndex);
			continue;
		}

		if (DoesPawnMatchShownPage(*CandidatePawn))
		{
			RemotePawnsOnShownPage.Add(CandidatePawn);
		}
	}

	PawnMarkers.UpdateRemoteMarks(MakeMarkerCanvas(), RemotePawnsOnShownPage);
}

void UMapWidget::HideDesignerMarkerWidgets() const
{
	if (CharacterMark)
	{
		CharacterMark->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (TeamMark)
	{
		TeamMark->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (EnemyMark)
	{
		EnemyMark->SetVisibility(ESlateVisibility::Collapsed);
	}
}

// 표식 묶음이 지금 보이는 지도 기준으로 표식을 만들고 놓을 수 있게 캔버스와 투영 정보를 모은다.
FMapMarkerCanvas UMapWidget::MakeMarkerCanvas()
{
	FMapMarkerCanvas Canvas;
	Canvas.Outer = this;
	Canvas.ParentPanel = GetMarkerParentPanel();
	Canvas.MarkImages = &MarkImages;
	if (Layout.Pages.IsValidIndex(ShownPageIndex))
	{
		Canvas.Projection.Page = Layout.Pages[ShownPageIndex];
	}
	Canvas.Projection.MapSize = GetShownViewSize();
	Canvas.Projection.bRotateToForward = bRotateMarksToPawnForward;
	Canvas.Projection.RotationOffsetDegrees = MarkerRotationOffsetDegrees;
	return Canvas;
}

UPanelWidget* UMapWidget::GetMarkerParentPanel() const
{
	if (MarkerLayer)
	{
		return MarkerLayer.Get();
	}

	if (MapImage)
	{
		for (UPanelWidget* ParentPanel = MapImage->GetParent(); ParentPanel; ParentPanel = ParentPanel->GetParent())
		{
			if (UCanvasPanel* CanvasParent = Cast<UCanvasPanel>(ParentPanel))
			{
				return CanvasParent;
			}
		}

		if (UPanelWidget* MapParent = MapImage->GetParent())
		{
			return MapParent;
		}
	}

	return nullptr;
}

FMapAreaRegionBoxes UMapWidget::GetAreaRegionBoxes() const
{
	FMapAreaRegionBoxes RegionBoxes;
	RegionBoxes.Dome = DomeMarkers;
	RegionBoxes.Windmill = WindmillMarkers;
	RegionBoxes.Temple = TempleMarkers;
	return RegionBoxes;
}

// 지도가 한 장이면 모든 플레이어가 그 지도에 있다. 여러 장이면 지역 트리거가 정한 플레이어 지역의 지도다.
int32 UMapWidget::FindPlayerPageIndex(const APawn& Pawn) const
{
	if (Layout.Pages.Num() == 1)
	{
		return 0;
	}
	const APdPlayerState* PdPlayerState = Pawn.GetPlayerState<APdPlayerState>();
	return PdPlayerState ? Layout.FindPageIndex(PdPlayerState->GetPlayerMatchComponent()->GetPlayerMapRegion()) : INDEX_NONE;
}

bool UMapWidget::DoesPawnMatchShownPage(const APawn& Pawn) const
{
	return ShownPageIndex != INDEX_NONE && FindPlayerPageIndex(Pawn) == ShownPageIndex;
}

// 전체 지도는 정사각형, 지역 지도는 월드 비율 그대로 가장 큰 화면 크기 안에 넣는다.
FVector2D UMapWidget::GetShownViewSize() const
{
	if (!Layout.Pages.IsValidIndex(ShownPageIndex))
	{
		return FVector2D(FMath::Min(MaxMapViewSize.X, MaxMapViewSize.Y));
	}
	return Layout.Pages[ShownPageIndex].FitViewSize(MaxMapViewSize);
}

FText UMapWidget::GetShownMapName() const
{
	if (!Layout.Pages.IsValidIndex(ShownPageIndex))
	{
		return MenuText(TEXT("Info.AreaMap"));
	}
	const FName NameKey = Layout.Pages[ShownPageIndex].NameKey;
	return NameKey.IsNone() ? LevelName : MenuText(NameKey);
}

void UMapWidget::OnMenuLanguageChanged()
{
	if (IsConstructed())
	{
		ShowPage(ShownPageIndex);
	}
}
