#include "UI/Info/Map/MapWidget.h"

#include "Components/Button.h"
#include "Components/CanvasPanel.h"
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

#include UE_INLINE_GENERATED_CPP_BY_NAME(MapWidget)

namespace
{
	UMapWidget::EMapView MapRegionToView(const EPlayerMapRegion MapRegion)
	{
		switch (MapRegion)
		{
		case EPlayerMapRegion::Dome:
			return UMapWidget::EMapView::Dome;
		case EPlayerMapRegion::Temple:
			return UMapWidget::EMapView::Temple;
		case EPlayerMapRegion::Windmill:
			return UMapWidget::EMapView::Windmill;
		default:
			return UMapWidget::EMapView::Dome;
		}
	}
}

UMapWidget::UMapWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMapWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (IsDesignTime())
	{
		return;
	}

	ApplyWidgetDefinitionSettings();
	ResolveDefaultTextures();
	ApplyMapView(CurrentMapView);
	HideDesignerMarkerWidgets();
}

void UMapWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ApplyWidgetDefinitionSettings();
	ResolveDefaultTextures();
	HideDesignerMarkerWidgets();

	if (WindmillButton)
	{
		WindmillButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnWindmillButtonClicked);
	}

	if (DomeButton)
	{
		DomeButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnDomeButtonClicked);
	}

	if (TempleButton)
	{
		TempleButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnTempleButtonClicked);
	}

	if (SurfaceButton)
	{
		SurfaceButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnWindmillButtonClicked);
	}

	if (UndergroundButton)
	{
		UndergroundButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnDomeButtonClicked);
	}

	if (AreaMapButton)
	{
		AreaMapButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnAreaMapButtonClicked);
	}

	SyncMapViewToPlayerMapRegion();
	if (!bHasSyncedPlayerMapView)
	{
		ApplyMapView(CurrentMapView);
	}

	RefreshRemotePlayerPawns();
	HandleMapUpdateTick();
	StartMapUpdateTimers();
}

void UMapWidget::NativeDestruct()
{
	StopMapUpdateTimers();

	if (WindmillButton)
	{
		WindmillButton->OnClicked.RemoveDynamic(this, &ThisClass::OnWindmillButtonClicked);
	}

	if (DomeButton)
	{
		DomeButton->OnClicked.RemoveDynamic(this, &ThisClass::OnDomeButtonClicked);
	}

	if (TempleButton)
	{
		TempleButton->OnClicked.RemoveDynamic(this, &ThisClass::OnTempleButtonClicked);
	}

	if (SurfaceButton)
	{
		SurfaceButton->OnClicked.RemoveDynamic(this, &ThisClass::OnWindmillButtonClicked);
	}

	if (UndergroundButton)
	{
		UndergroundButton->OnClicked.RemoveDynamic(this, &ThisClass::OnDomeButtonClicked);
	}

	if (AreaMapButton)
	{
		AreaMapButton->OnClicked.RemoveDynamic(this, &ThisClass::OnAreaMapButtonClicked);
	}

	PawnMarkers.RemoveAll();
	CachedRemotePlayerPawns.Reset();
	AreaRegionMarkers.Reset();

	Super::NativeDestruct();
}

void UMapWidget::ShowAreaMap()
{
	bHasManualMapViewSelection = true;
	ApplyMapView(EMapView::Area);
}

void UMapWidget::ShowWindmillMap()
{
	bHasManualMapViewSelection = true;
	ApplyMapView(EMapView::Windmill);
}

void UMapWidget::ShowDomeMap()
{
	bHasManualMapViewSelection = true;
	ApplyMapView(EMapView::Dome);
}

void UMapWidget::ShowTempleMap()
{
	bHasManualMapViewSelection = true;
	ApplyMapView(EMapView::Temple);
}

void UMapWidget::OnWindmillButtonClicked()
{
	ShowWindmillMap();
}

void UMapWidget::OnDomeButtonClicked()
{
	ShowDomeMap();
}

void UMapWidget::OnTempleButtonClicked()
{
	ShowTempleMap();
}

void UMapWidget::OnAreaMapButtonClicked()
{
	ShowAreaMap();
}

// 지역 지도에서는 그 지역의 플레이어 표식을, 전체 지도에서는 지역 선택 버튼과 지역별 표식 줄을 보인다.
void UMapWidget::ApplyMapView(const EMapView NewMapView)
{
	CurrentMapView = NewMapView;
	ResolveDefaultTextures();

	const bool bAreaView = CurrentMapView == EMapView::Area;
	ApplyMapTexture(GetMapTexture(CurrentMapView));
	ApplyTotalSizeBoxSize(GetCurrentTotalSizeBoxSize());
	if (MapName)
	{
		MapName->SetText(GetMapViewName(CurrentMapView));
	}
	SetRegionSelectionButtonsVisible(bAreaView);
	if (AreaMapButton)
	{
		AreaMapButton->SetVisibility(bAreaView ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
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

void UMapWidget::ApplyMapTexture(UTexture2D* Texture)
{
	if (!MapImage || !Texture)
	{
		return;
	}

	MapImage->SetBrushFromTexture(Texture, true);
}

void UMapWidget::ApplyTotalSizeBoxSize(const FVector2D& Size)
{
	if (!TotalSizeBox)
	{
		return;
	}

	TotalSizeBox->SetWidthOverride(FMath::Max(Size.X, 1.0f));
	TotalSizeBox->SetHeightOverride(FMath::Max(Size.Y, 1.0f));
}

void UMapWidget::ResolveDefaultTextures()
{
	if (!TempleMapTexture)
	{
		TempleMapTexture = WindmillMapTexture;
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
	if (UTexture2D* LoadedAreaMapTexture = Settings.AreaMapTexture.Get())
	{
		AreaMapTexture = LoadedAreaMapTexture;
	}
	if (UTexture2D* LoadedWindmillMapTexture = Settings.WindmillMapTexture.Get())
	{
		WindmillMapTexture = LoadedWindmillMapTexture;
	}
	if (UTexture2D* LoadedDomeMapTexture = Settings.DomeMapTexture.Get())
	{
		DomeMapTexture = LoadedDomeMapTexture;
	}
	if (UTexture2D* LoadedTempleMapTexture = Settings.TempleMapTexture.Get())
	{
		TempleMapTexture = LoadedTempleMapTexture;
	}

	bShowRemotePlayerMarks = Settings.bShowRemotePlayerMarks;
	MarkerUpdateInterval = FMath::Max(Settings.MarkerUpdateInterval, 0.02f);
	RemotePlayerListRefreshInterval = FMath::Max(Settings.RemotePlayerListRefreshInterval, 0.1f);
	MarkImages.Configure(Settings);

	if (const FMapWidgetProjectionSettings* ProjectionOverride = FindProjectionOverride(Settings))
	{
		ProjectionSettings = *ProjectionOverride;
	}
	else if (Settings.bUseDefaultProjectionSettings)
	{
		ProjectionSettings = Settings.DefaultProjectionSettings;
	}
}

const FMapWidgetProjectionSettings* UMapWidget::FindProjectionOverride(const FMapWidgetSettings& Settings) const
{
	const UClass* ThisWidgetClass = GetClass();
	if (!ThisWidgetClass)
	{
		return nullptr;
	}

	for (const FMapWidgetProjectionOverride& ProjectionOverride : Settings.ProjectionOverrides)
	{
		UClass* OverrideWidgetClass = ProjectionOverride.MapWidgetClass.Get();
		if (!OverrideWidgetClass)
		{
			continue;
		}

		if (ThisWidgetClass == OverrideWidgetClass || ThisWidgetClass->IsChildOf(OverrideWidgetClass))
		{
			return &ProjectionOverride.ProjectionSettings;
		}
	}

	return nullptr;
}

void UMapWidget::SetRegionSelectionButtonsVisible(const bool bVisible)
{
	const ESlateVisibility NewVisibility = bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;

	if (WindmillButton)
	{
		WindmillButton->SetVisibility(NewVisibility);
	}

	if (DomeButton)
	{
		DomeButton->SetVisibility(NewVisibility);
	}

	if (TempleButton)
	{
		TempleButton->SetVisibility(NewVisibility);
	}

	if (SurfaceButton)
	{
		SurfaceButton->SetVisibility(NewVisibility);
	}

	if (UndergroundButton)
	{
		UndergroundButton->SetVisibility(NewVisibility);
	}
}

void UMapWidget::SyncMapViewToPlayerMapRegion()
{
	const APawn* OwningPawn = GetOwningPlayerPawn();
	const APdPlayerState* PdPlayerState = OwningPawn ? OwningPawn->GetPlayerState<APdPlayerState>() : nullptr;
	if (!PdPlayerState)
	{
		return;
	}

	const EMapView PlayerMapView = MapRegionToView(PdPlayerState->GetPlayerMatchComponent()->GetPlayerMapRegion());

	if (bHasManualMapViewSelection)
	{
		if (bHasSyncedPlayerMapView && LastSyncedPlayerMapView == PlayerMapView)
		{
			return;
		}

		bHasManualMapViewSelection = false;
	}

	const bool bIsShowingAllowedMapView = CurrentMapView == PlayerMapView || CurrentMapView == EMapView::Area;
	if (bHasSyncedPlayerMapView && LastSyncedPlayerMapView == PlayerMapView && bIsShowingAllowedMapView)
	{
		return;
	}

	bHasSyncedPlayerMapView = true;
	LastSyncedPlayerMapView = PlayerMapView;
	ApplyMapView(PlayerMapView);
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
	SyncMapViewToPlayerMapRegion();
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
	const bool bShowAreaMarkers = CurrentMapView == EMapView::Area;
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
	if (CurrentMapView == EMapView::Area)
	{
		PawnMarkers.HideSelfMarks();
		return;
	}

	const APawn* OwningPawn = GetOwningPlayerPawn();
	if (!OwningPawn)
	{
		PawnMarkers.HideSelfMarks();
		return;
	}

	PawnMarkers.UpdateSelfMarks(MakeMarkerCanvas(),
		*OwningPawn,
		DoesPawnMatchCurrentMapView(*OwningPawn));
}

void UMapWidget::UpdateTeamMarks()
{
	if (!bShowRemotePlayerMarks || CurrentMapView == EMapView::Area)
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

	TArray<const APawn*> RemotePawnsOnShownRegion;
	RemotePawnsOnShownRegion.Reserve(CachedRemotePlayerPawns.Num());
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

		if (!DoesPawnMatchCurrentMapView(*CandidatePawn))
		{
			continue;
		}

		RemotePawnsOnShownRegion.Add(CandidatePawn);
	}

	PawnMarkers.UpdateRemoteMarks(MakeMarkerCanvas(), RemotePawnsOnShownRegion);
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

// 표식 묶음이 현재 지역 지도 기준으로 표식을 만들고 놓을 수 있게 캔버스와 투영 정보를 모은다.
FMapMarkerCanvas UMapWidget::MakeMarkerCanvas()
{
	FMapMarkerCanvas Canvas;
	Canvas.Outer = this;
	Canvas.ParentPanel = GetMarkerParentPanel();
	Canvas.MarkImages = &MarkImages;
	Canvas.Projection.PlaneTransform = GetCurrentPlaneTransform();
	Canvas.Projection.MapSize = GetCurrentTotalSizeBoxSize();
	Canvas.Projection.PlaneLocalSize = ProjectionSettings.PlaneLocalSize;
	Canvas.Projection.bRotateToForward = ProjectionSettings.bRotateMarksToPawnForward;
	Canvas.Projection.RotationOffsetDegrees = ProjectionSettings.MarkerRotationOffsetDegrees;
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

bool UMapWidget::DoesPawnMatchCurrentMapView(const APawn& Pawn) const
{
	if (CurrentMapView == EMapView::Area)
	{
		return false;
	}

	const APdPlayerState* PdPlayerState = Pawn.GetPlayerState<APdPlayerState>();
	if (!PdPlayerState)
	{
		return false;
	}

	return MapRegionToView(PdPlayerState->GetPlayerMatchComponent()->GetPlayerMapRegion()) == CurrentMapView;
}

UTexture2D* UMapWidget::GetMapTexture(const EMapView MapView) const
{
	switch (MapView)
	{
	case EMapView::Windmill:
		return WindmillMapTexture;
	case EMapView::Dome:
		return DomeMapTexture;
	case EMapView::Temple:
		return TempleMapTexture;
	case EMapView::Area:
	default:
		return AreaMapTexture;
	}
}

FText UMapWidget::GetMapViewName(const EMapView MapView) const
{
	switch (MapView)
	{
	case EMapView::Windmill:
		return MenuTextOrFallback(TEXT("Info.Windmill"), FText::FromString(TEXT("Windmill")));
	case EMapView::Dome:
		return MenuTextOrFallback(TEXT("Info.Dome"), FText::FromString(TEXT("Dome")));
	case EMapView::Temple:
		return MenuTextOrFallback(TEXT("Info.Temple"), FText::FromString(TEXT("Temple")));
	case EMapView::Area:
	default:
		return MenuTextOrFallback(TEXT("Info.AreaMap"), FText::FromString(TEXT("Area Map")));
	}
}

const FTransform& UMapWidget::GetCurrentPlaneTransform() const
{
	switch (CurrentMapView)
	{
	case EMapView::Dome:
		return ProjectionSettings.DomePlaneTransform;
	case EMapView::Temple:
		return ProjectionSettings.TemplePlaneTransform;
	case EMapView::Windmill:
	case EMapView::Area:
	default:
		return ProjectionSettings.WindmillPlaneTransform;
	}
}

FVector2D UMapWidget::GetCurrentTotalSizeBoxSize() const
{
	switch (CurrentMapView)
	{
	case EMapView::Windmill:
		return ProjectionSettings.WindmillMapTotalSizeBoxSize;
	case EMapView::Dome:
		return ProjectionSettings.DomeMapTotalSizeBoxSize;
	case EMapView::Temple:
		return ProjectionSettings.TempleMapTotalSizeBoxSize;
	case EMapView::Area:
	default:
		return ProjectionSettings.AreaMapTotalSizeBoxSize;
	}
}

void UMapWidget::OnMenuLanguageChanged()
{
	ApplyMapView(CurrentMapView);
}
