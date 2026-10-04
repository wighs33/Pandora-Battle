#pragma once

#include "UI/Common/LocalizedMenuWidget.h"
#include "UI/Core/WidgetClassDefinition.h"
#include "UI/Info/Map/MapAreaRegionMarkers.h"
#include "UI/Info/Map/MapMarkImages.h"
#include "UI/Info/Map/MapPawnMarkers.h"
#include "MapWidget.generated.h"

class UButton;
class UCanvasPanel;
class UHorizontalBox;
class UImage;
class UPanelWidget;
class USizeBox;
class UTextBlock;
class UTexture2D;
class APawn;

// 지역 지도와 전체 지도를 바꿔 보여 주는 위젯. 보기 전환·설정·갱신 주기를 맡고,
// 표식 그림 고르기와 내·다른 플레이어 표식, 전체 지도의 지역별 표식은 각 표식 묶음에 맡긴다.
UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UMapWidget : public ULocalizedMenuWidget
{
	GENERATED_BODY()

public:
	enum class EMapView : uint8
	{
		Area,
		Windmill,
		Dome,
		Temple
	};

protected:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

public:
	// Public API ------------------------------------------------------------------------------------------------------
	UMapWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "!UI|Map")
	void ShowAreaMap();

	UFUNCTION(BlueprintCallable, Category = "!UI|Map")
	void ShowWindmillMap();

	UFUNCTION(BlueprintCallable, Category = "!UI|Map")
	void ShowDomeMap();

	UFUNCTION(BlueprintCallable, Category = "!UI|Map")
	void ShowTempleMap();

protected:
	// Event Handlers --------------------------------------------------------------------------------------------------
	virtual void OnMenuLanguageChanged() override;

private:
	UFUNCTION()
	void OnWindmillButtonClicked();

	UFUNCTION()
	void OnDomeButtonClicked();

	UFUNCTION()
	void OnTempleButtonClicked();

	UFUNCTION()
	void OnAreaMapButtonClicked();
	void HandleMapUpdateTick();
	void RefreshRemotePlayerPawns();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void ApplyMapView(EMapView NewMapView);
	void ApplyMapTexture(UTexture2D* Texture);
	void ApplyTotalSizeBoxSize(const FVector2D& Size);
	void ResolveDefaultTextures();
	void ApplyWidgetDefinitionSettings();
	const FMapWidgetProjectionSettings* FindProjectionOverride(const FMapWidgetSettings& Settings) const;
	void SetRegionSelectionButtonsVisible(bool bVisible);
	void SyncMapViewToPlayerMapRegion();
	void StartMapUpdateTimers();
	void StopMapUpdateTimers();
	void RefreshAreaMapRegionMarkers();
	void UpdateCharacterMark();
	void UpdateTeamMarks();
	void HideDesignerMarkerWidgets() const;
	FMapMarkerCanvas MakeMarkerCanvas();
	UPanelWidget* GetMarkerParentPanel() const;
	FMapAreaRegionBoxes GetAreaRegionBoxes() const;
	bool DoesPawnMatchCurrentMapView(const APawn& Pawn) const;
	UTexture2D* GetMapTexture(EMapView MapView) const;
	FText GetMapViewName(EMapView MapView) const;
	const FTransform& GetCurrentPlaneTransform() const;
	FVector2D GetCurrentTotalSizeBoxSize() const;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "!UI|Map", meta = (BindWidgetOptional))
	TObjectPtr<UButton> WindmillButton;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Map", meta = (BindWidgetOptional))
	TObjectPtr<UButton> DomeButton;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Map", meta = (BindWidgetOptional))
	TObjectPtr<UButton> TempleButton;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Map", meta = (BindWidgetOptional))
	TObjectPtr<UButton> SurfaceButton;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Map", meta = (BindWidgetOptional))
	TObjectPtr<UButton> UndergroundButton;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Map", meta = (BindWidgetOptional))
	TObjectPtr<UButton> AreaMapButton;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Map", meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> TotalSizeBox;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Map", meta = (BindWidgetOptional))
	TObjectPtr<UImage> MapImage;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Map", meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> MarkerLayer;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Map|Area", meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> DomeMarkers;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Map|Area", meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> WindmillMarkers;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Map|Area", meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> TempleMarkers;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Map", meta = (BindWidgetOptional))
	TObjectPtr<UImage> CharacterMark;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Map", meta = (BindWidgetOptional))
	TObjectPtr<UImage> TeamMark;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Map", meta = (BindWidgetOptional))
	TObjectPtr<UImage> EnemyMark;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Map", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MapName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Map|Texture")
	TObjectPtr<UTexture2D> AreaMapTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Map|Texture")
	TObjectPtr<UTexture2D> WindmillMapTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Map|Texture")
	TObjectPtr<UTexture2D> DomeMapTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Map|Texture")
	TObjectPtr<UTexture2D> TempleMapTexture;

	// 지도별 크기와 지역 평면. 정의 데이터의 위젯별 덮어쓰기나 기본값으로 채운다.
	UPROPERTY(BlueprintReadOnly, Category = "!UI|Map|Projection")
	FMapWidgetProjectionSettings ProjectionSettings;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Map|Marker")
	bool bShowRemotePlayerMarks = true;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Map|Marker", meta = (ClampMin = "0.02", ForceUnits = "s"))
	float MarkerUpdateInterval = 0.1f;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Map|Marker", meta = (ClampMin = "0.1", ForceUnits = "s"))
	float RemotePlayerListRefreshInterval = 0.5f;

private:
	UPROPERTY(Transient)
	FMapPawnMarkers PawnMarkers;

	FMapMarkImages MarkImages;
	FMapAreaRegionMarkers AreaRegionMarkers;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<APawn>> CachedRemotePlayerPawns;

	FTimerHandle MarkerUpdateTimerHandle;
	FTimerHandle RemotePlayerListRefreshTimerHandle;

	EMapView CurrentMapView = EMapView::Dome;
	EMapView LastSyncedPlayerMapView = EMapView::Dome;
	bool bHasSyncedPlayerMapView = false;
	bool bHasManualMapViewSelection = false;
};
