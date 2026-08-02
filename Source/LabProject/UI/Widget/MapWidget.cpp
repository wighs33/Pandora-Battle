#include "UI/Widget/MapWidget.h"

#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "EngineUtils.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Pawn.h"
#include "Mode/PdPlayerState.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MapWidget)

namespace
{
	bool HasPositiveSize(const FVector2D& Size)
	{
		return Size.X > 0.0f && Size.Y > 0.0f;
	}

	int32 TeamColorToIndex(const EPdTeamColor TeamColor)
	{
		return static_cast<int32>(TeamColor);
	}

	FVector2D GetDefaultMarkerSize()
	{
		return FVector2D(32.0f, 32.0f);
	}

	UMapWidget::EMapView MapRegionToView(const EPdPlayerMapRegion MapRegion)
	{
		switch (MapRegion)
		{
		case EPdPlayerMapRegion::Dome:
			return UMapWidget::EMapView::Dome;
		case EPdPlayerMapRegion::Temple:
			return UMapWidget::EMapView::Temple;
		case EPdPlayerMapRegion::Windmill:
		default:
			return UMapWidget::EMapView::Windmill;
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

	if (SelfTeamMarkWidget)
	{
		SelfTeamMarkWidget->RemoveFromParent();
		SelfTeamMarkWidget = nullptr;
	}

	if (SelfCharacterMarkWidget)
	{
		SelfCharacterMarkWidget->RemoveFromParent();
		SelfCharacterMarkWidget = nullptr;
	}

	for (UImage* DynamicTeamMark : RemoteTeamMarkWidgets)
	{
		if (DynamicTeamMark)
		{
			DynamicTeamMark->RemoveFromParent();
		}
	}
	RemoteTeamMarkWidgets.Reset();
	CachedRemotePlayerPawns.Reset();

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

void UMapWidget::ApplyMapView(const EMapView NewMapView)
{
	CurrentMapView = NewMapView;
	ResolveDefaultTextures();

	switch (CurrentMapView)
	{
	case EMapView::Windmill:
		ApplyMapTexture(WindmillMapTexture);
		ApplyTotalSizeBoxSize(WindmillMapTotalSizeBoxSize);
		if (MapName)
		{
			MapName->SetText(FText::FromString(TEXT("Windmill")));
		}
		SetRegionSelectionButtonsVisible(false);
		if (AreaMapButton)
		{
			AreaMapButton->SetVisibility(ESlateVisibility::Visible);
		}
		UpdateCharacterMark();
		break;
	case EMapView::Dome:
		ApplyMapTexture(DomeMapTexture);
		ApplyTotalSizeBoxSize(DomeMapTotalSizeBoxSize);
		if (MapName)
		{
			MapName->SetText(FText::FromString(TEXT("Dome")));
		}
		SetRegionSelectionButtonsVisible(false);
		if (AreaMapButton)
		{
			AreaMapButton->SetVisibility(ESlateVisibility::Visible);
		}
		UpdateCharacterMark();
		break;
	case EMapView::Temple:
		ApplyMapTexture(TempleMapTexture);
		ApplyTotalSizeBoxSize(TempleMapTotalSizeBoxSize);
		if (MapName)
		{
			MapName->SetText(FText::FromString(TEXT("Temple")));
		}
		SetRegionSelectionButtonsVisible(false);
		if (AreaMapButton)
		{
			AreaMapButton->SetVisibility(ESlateVisibility::Visible);
		}
		UpdateCharacterMark();
		break;
	case EMapView::Area:
	default:
		ApplyMapTexture(AreaMapTexture);
		ApplyTotalSizeBoxSize(AreaMapTotalSizeBoxSize);
		if (MapName)
		{
			MapName->SetText(FText::FromString(TEXT("Area Map")));
		}
		SetRegionSelectionButtonsVisible(true);
		if (AreaMapButton)
		{
			AreaMapButton->SetVisibility(ESlateVisibility::Collapsed);
		}
		HideSelfMarks();
		HideTeamMarks();
		break;
	}


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
	TeamMarkImagesByTeamColorIndex.Reset();
	TeamMarkImageSizesByTeamColorIndex.Reset();
	CharacterMarkResourceObject = Settings.CharacterMarkImage.Get();
	CharacterMarkResolvedImageSize = Settings.CharacterMarkImageSize;

	if (const FMapWidgetProjectionSettings* ProjectionOverride = FindProjectionOverride(Settings))
	{
		ApplyProjectionSettings(*ProjectionOverride);
	}
	else if (Settings.bUseDefaultProjectionSettings)
	{
		ApplyProjectionSettings(Settings.DefaultProjectionSettings);
	}

	for (const FMapWidgetTeamMarkImage& TeamMarkImage : Settings.TeamMarkImages)
	{
		const int32 TeamColorIndex = TeamColorToIndex(TeamMarkImage.TeamColor);
		if (UObject* ResourceObject = TeamMarkImage.TeamMarkImage.Get())
		{
			TeamMarkImagesByTeamColorIndex.Add(TeamColorIndex, ResourceObject);
		}

		if (HasPositiveSize(TeamMarkImage.TeamMarkImageSize))
		{
			TeamMarkImageSizesByTeamColorIndex.Add(TeamColorIndex, TeamMarkImage.TeamMarkImageSize);
		}
	}
}

void UMapWidget::ApplyProjectionSettings(const FMapWidgetProjectionSettings& ProjectionSettings)
{
	AreaMapTotalSizeBoxSize = ProjectionSettings.AreaMapTotalSizeBoxSize;
	WindmillMapTotalSizeBoxSize = ProjectionSettings.WindmillMapTotalSizeBoxSize;
	DomeMapTotalSizeBoxSize = ProjectionSettings.DomeMapTotalSizeBoxSize;
	TempleMapTotalSizeBoxSize = ProjectionSettings.TempleMapTotalSizeBoxSize;
	WindmillPlaneTransform = ProjectionSettings.WindmillPlaneTransform;
	TemplePlaneTransform = ProjectionSettings.TemplePlaneTransform;
	DomePlaneTransform = ProjectionSettings.DomePlaneTransform;
	PlaneLocalSize = FMath::Max(ProjectionSettings.PlaneLocalSize, 1.0f);
	bRotateMarksToPawnForward = ProjectionSettings.bRotateMarksToPawnForward;
	MarkerRotationOffsetDegrees = ProjectionSettings.MarkerRotationOffsetDegrees;
}

bool UMapWidget::ApplyMarkImage(UImage* MarkWidget, UObject* ResourceObject, const FVector2D& DesiredImageSize) const
{
	if (!MarkWidget || !ResourceObject)
	{
		return false;
	}

	FSlateBrush MarkBrush = MarkWidget->GetBrush();
	MarkBrush.SetResourceObject(ResourceObject);

	if (HasPositiveSize(DesiredImageSize))
	{
		MarkBrush.ImageSize = DesiredImageSize;
	}
	else if (!HasPositiveSize(MarkBrush.ImageSize))
	{
		MarkBrush.ImageSize = GetDefaultMarkerSize();
	}

	MarkWidget->SetBrush(MarkBrush);

	const FVector2D SlotSize = HasPositiveSize(DesiredImageSize) ? DesiredImageSize : MarkBrush.ImageSize;
	if (HasPositiveSize(SlotSize))
	{
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(MarkWidget->Slot))
		{
			CanvasSlot->SetSize(SlotSize);
		}
	}

	return true;
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

	const EMapView PlayerMapView = MapRegionToView(
		PdPlayerState->GetPlayerMatchComponent()->GetPlayerMapRegion());

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

	World->GetTimerManager().SetTimer(
		MarkerUpdateTimerHandle,
		this,
		&ThisClass::HandleMapUpdateTick,
		FMath::Max(MarkerUpdateInterval, 0.02f),
		true);

	World->GetTimerManager().SetTimer(
		RemotePlayerListRefreshTimerHandle,
		this,
		&ThisClass::RefreshRemotePlayerPawns,
		FMath::Max(RemotePlayerListRefreshInterval, 0.1f),
		true);
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

void UMapWidget::UpdateCharacterMark()
{
	if (CurrentMapView == EMapView::Area)
	{
		HideSelfMarks();
		return;
	}

	APawn* OwningPawn = GetOwningPlayerPawn();
	if (!OwningPawn)
	{
		HideSelfMarks();
		return;
	}

	bool bTeamMarkVisible = false;
	if (!SelfTeamMarkWidget)
	{
		SelfTeamMarkWidget = CreateDynamicMarkerWidget(TEXT("SelfTeamMark"), 10010);
	}

	if (SelfTeamMarkWidget)
	{
		bTeamMarkVisible = ApplyTeamMarkImage(SelfTeamMarkWidget, *OwningPawn)
			&& UpdatePawnMapMark(SelfTeamMarkWidget, *OwningPawn);

		if (!bTeamMarkVisible)
		{
			SelfTeamMarkWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (!SelfCharacterMarkWidget)
	{
		SelfCharacterMarkWidget = CreateDynamicMarkerWidget(TEXT("SelfCharacterMark"), 10011);
	}

	if (SelfCharacterMarkWidget)
	{
		const bool bCharacterMarkVisible = bTeamMarkVisible
			&& ApplyCharacterMarkImage(SelfCharacterMarkWidget)
			&& UpdatePawnMapMark(SelfCharacterMarkWidget, *OwningPawn, false);

		if (!bCharacterMarkVisible)
		{
			SelfCharacterMarkWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UMapWidget::UpdateTeamMarks()
{
	if (!bShowRemotePlayerMarks || CurrentMapView == EMapView::Area)
	{
		HideTeamMarks();
		return;
	}

	APawn* OwningPawn = GetOwningPlayerPawn();
	const APdPlayerState* OwningPlayerState = OwningPawn ? OwningPawn->GetPlayerState<APdPlayerState>() : nullptr;
	if (!OwningPawn || !OwningPlayerState)
	{
		HideTeamMarks();
		return;
	}

	TArray<APawn*> RemotePlayerPawns;
	RemotePlayerPawns.Reserve(CachedRemotePlayerPawns.Num());
	for (int32 PawnIndex = CachedRemotePlayerPawns.Num() - 1; PawnIndex >= 0; --PawnIndex)
	{
		APawn* CandidatePawn = CachedRemotePlayerPawns[PawnIndex].Get();
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

		RemotePlayerPawns.Add(CandidatePawn);
	}

	EnsureTeamMarkCapacity(RemotePlayerPawns.Num());

	int32 VisibleMarkCount = 0;
	for (APawn* RemotePlayerPawn : RemotePlayerPawns)
	{
		if (!RemoteTeamMarkWidgets.IsValidIndex(VisibleMarkCount))
		{
			break;
		}

		UImage* CurrentTeamMark = RemoteTeamMarkWidgets[VisibleMarkCount];
		if (ApplyTeamMarkImage(CurrentTeamMark, *RemotePlayerPawn)
			&& UpdatePawnMapMark(CurrentTeamMark, *RemotePlayerPawn))
		{
			++VisibleMarkCount;
		}
		else if (CurrentTeamMark)
		{
			CurrentTeamMark->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	for (int32 Index = VisibleMarkCount; Index < RemoteTeamMarkWidgets.Num(); ++Index)
	{
		if (UImage* CurrentTeamMark = RemoteTeamMarkWidgets[Index])
		{
			CurrentTeamMark->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UMapWidget::HideTeamMarks()
{
	for (UImage* CurrentTeamMark : RemoteTeamMarkWidgets)
	{
		if (CurrentTeamMark)
		{
			CurrentTeamMark->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UMapWidget::HideSelfMarks()
{
	if (SelfTeamMarkWidget)
	{
		SelfTeamMarkWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (SelfCharacterMarkWidget)
	{
		SelfCharacterMarkWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
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

void UMapWidget::EnsureTeamMarkCapacity(const int32 RequiredCount)
{
	if (RequiredCount <= RemoteTeamMarkWidgets.Num())
	{
		return;
	}

	while (RemoteTeamMarkWidgets.Num() < RequiredCount)
	{
		UImage* NewTeamMark = CreateDynamicMarkerWidget(
			*FString::Printf(TEXT("RemoteTeamMark_%d"), RemoteTeamMarkWidgets.Num()),
			10000);
		if (!NewTeamMark)
		{
			return;
		}

		RemoteTeamMarkWidgets.Add(NewTeamMark);
	}
}

UImage* UMapWidget::CreateDynamicMarkerWidget(const FName MarkerName, const int32 ZOrder)
{
	UPanelWidget* ParentPanel = GetMarkerParentPanel();
	if (!ParentPanel)
	{
		return nullptr;
	}

	UImage* NewMarker = NewObject<UImage>(this, UImage::StaticClass(), MarkerName);
	if (!NewMarker)
	{
		return nullptr;
	}

	NewMarker->SetColorAndOpacity(FLinearColor::White);
	NewMarker->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	NewMarker->SetVisibility(ESlateVisibility::Collapsed);

	if (UCanvasPanel* CanvasParent = Cast<UCanvasPanel>(ParentPanel))
	{
		UCanvasPanelSlot* CanvasSlot = CanvasParent->AddChildToCanvas(NewMarker);
		if (!CanvasSlot)
		{
			return nullptr;
		}

		CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f));
		CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		CanvasSlot->SetAutoSize(false);
		CanvasSlot->SetSize(GetDefaultMarkerSize());
		CanvasSlot->SetZOrder(ZOrder);
	}
	else
	{
		if (!ParentPanel->AddChild(NewMarker))
		{
			return nullptr;
		}
	}

	return NewMarker;
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

bool UMapWidget::UpdatePawnMapMark(UImage* MarkWidget, const APawn& Pawn, const bool bRotateToPawnForward) const
{
	if (!MarkWidget || CurrentMapView == EMapView::Area)
	{
		return false;
	}

	const APdPlayerState* PdPlayerState = Pawn.GetPlayerState<APdPlayerState>();
	if (!PdPlayerState)
	{
		return false;
	}

	if (!DoesPawnMatchCurrentMapView(Pawn))
	{
		return false;
	}

	const FTransform& PlaneTransform = GetCurrentPlaneTransform();
	const FVector PlaneLocalLocation = PlaneTransform.InverseTransformPosition(Pawn.GetActorLocation());
	const float SafePlaneLocalSize = FMath::Max(PlaneLocalSize, 1.0f);
	const float NormalizedX = FMath::Clamp((PlaneLocalLocation.X / SafePlaneLocalSize) + 0.5f, 0.0f, 1.0f);
	const float NormalizedY = FMath::Clamp((PlaneLocalLocation.Y / SafePlaneLocalSize) + 0.5f, 0.0f, 1.0f);
	const FVector2D MapSize = GetCurrentTotalSizeBoxSize();
	const FVector2D MarkPosition(NormalizedX * MapSize.X, NormalizedY * MapSize.Y);

	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(MarkWidget->Slot))
	{
		CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f));
		CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		CanvasSlot->SetPosition(MarkPosition);
	}
	else
	{
		MarkWidget->SetRenderTranslation(MarkPosition);
	}

	float RotationAngle = bRotateToPawnForward ? MarkerRotationOffsetDegrees : 0.0f;
	if (bRotateToPawnForward && bRotateMarksToPawnForward)
	{
		const FVector PlaneLocalForward = PlaneTransform.InverseTransformVectorNoScale(Pawn.GetActorForwardVector());
		FVector2D ScreenForward(PlaneLocalForward.X, PlaneLocalForward.Y);
		if (!ScreenForward.Normalize())
		{
			ScreenForward = FVector2D(0.0f, 1.0f);
		}

		RotationAngle += -FMath::RadiansToDegrees(FMath::Atan2(ScreenForward.X, ScreenForward.Y));
	}

	MarkWidget->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	MarkWidget->SetRenderTransformAngle(RotationAngle);
	MarkWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	return true;
}

bool UMapWidget::ApplyCharacterMarkImage(UImage* MarkWidget) const
{
	return ApplyMarkImage(MarkWidget, CharacterMarkResourceObject.Get(), CharacterMarkResolvedImageSize);
}

bool UMapWidget::ApplyTeamMarkImage(UImage* MarkWidget, const APawn& Pawn) const
{
	if (!MarkWidget)
	{
		return false;
	}

	const APdPlayerState* PdPlayerState = Pawn.GetPlayerState<APdPlayerState>();
	if (!PdPlayerState)
	{
		return false;
	}

	const int32 TeamColorIndex = FMath::Clamp(
		PdPlayerState->GetPlayerMatchComponent()->GetMatchTeamColorIndex(),
		0,
		TeamColorToIndex(EPdTeamColor::Orange));
	const TObjectPtr<UObject>* FoundResource = TeamMarkImagesByTeamColorIndex.Find(TeamColorIndex);
	UObject* ResourceObject = FoundResource ? FoundResource->Get() : nullptr;
	if (!ResourceObject)
	{
		return false;
	}

	FVector2D DesiredImageSize = FVector2D::ZeroVector;
	if (const FVector2D* FoundImageSize = TeamMarkImageSizesByTeamColorIndex.Find(TeamColorIndex))
	{
		DesiredImageSize = *FoundImageSize;
	}

	return ApplyMarkImage(MarkWidget, ResourceObject, DesiredImageSize);
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

const FTransform& UMapWidget::GetCurrentPlaneTransform() const
{
	switch (CurrentMapView)
	{
	case EMapView::Dome:
		return DomePlaneTransform;
	case EMapView::Temple:
		return TemplePlaneTransform;
	case EMapView::Windmill:
	case EMapView::Area:
	default:
		return WindmillPlaneTransform;
	}
}

FVector2D UMapWidget::GetCurrentTotalSizeBoxSize() const
{
	switch (CurrentMapView)
	{
	case EMapView::Windmill:
		return WindmillMapTotalSizeBoxSize;
	case EMapView::Dome:
		return DomeMapTotalSizeBoxSize;
	case EMapView::Temple:
		return TempleMapTotalSizeBoxSize;
	case EMapView::Area:
	default:
		return AreaMapTotalSizeBoxSize;
	}
}
