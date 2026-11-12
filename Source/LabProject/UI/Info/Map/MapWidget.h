#pragma once

#include "Definition/Level/LevelMapLayout.h"
#include "UI/Common/LocalizedMenuWidget.h"
#include "UI/Core/WidgetClassDefinition.h"
#include "UI/Info/Map/MapAreaRegionMarkers.h"
#include "UI/Info/Map/MapMarkImages.h"
#include "UI/Info/Map/MapPawnMarkers.h"
#include "MapWidget.generated.h"

class UButton;
class UButtonClickRelay;
class UCanvasPanel;
class UHorizontalBox;
class UImage;
class UPanelWidget;
class USizeBox;
class UTextBlock;
class UTexture2D;
class APawn;

// 레벨 지도(FLevelMapLayout)를 보여 주는 위젯. 지역 지도가 여럿이면 전체 지도에서 지역을 고르고, 한 장이면 그 지도만 보인다.
// 보기 전환·설정·갱신 주기를 맡고, 표식 그림 고르기와 내·다른 플레이어 표식, 전체 지도의 지역별 표식은 각 표식 묶음에 맡긴다.
UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UMapWidget : public ULocalizedMenuWidget
{
	GENERATED_BODY()

protected:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

public:
	// Public API ------------------------------------------------------------------------------------------------------
	UMapWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 보일 레벨 지도와 레벨 이름(이름 키가 없는 지도에 쓴다). 정보 창이 지도를 열 때 넘긴다. */
	void SetLevelMap(const FLevelMapLayout& InLayout, const FText& InLevelName);

	UFUNCTION(BlueprintCallable, Category = "!UI|Map")
	void ShowAreaMap();

	UFUNCTION(BlueprintCallable, Category = "!UI|Map")
	void ShowRegionMap(EPlayerMapRegion Region);

	/** 지금 보이는 지도: 전체 지도면 INDEX_NONE, 아니면 레벨 지도의 Pages 번호. */
	int32 GetShownPageIndex() const { return ShownPageIndex; }

	/** 전체 지도에서 그 지역의 빛이 켜져 있는지(지역 단추에 마우스가 올라가 있는지). */
	bool IsRegionHighlighted(EPlayerMapRegion Region) const;

protected:
	// Event Handlers --------------------------------------------------------------------------------------------------
	virtual void OnMenuLanguageChanged() override;

private:
	UFUNCTION()
	void OnAreaMapButtonClicked();

	void HandleRegionHoverChanged(bool bHovered, EPlayerMapRegion Region);
	void HandleMapUpdateTick();
	void RefreshRemotePlayerPawns();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void ApplyWidgetDefinitionSettings();
	void ApplyLevelMap();
	void ShowPage(int32 PageIndex);
	void BindRegionButtons();
	void UnbindRegionButtons();
	void PlaceAreaRegionWidgets();
	void SetRegionHighlightVisible(int32 PageIndex, bool bVisible);
	UButton* GetRegionButton(EPlayerMapRegion Region) const;
	void SetRegionSelectionVisible(bool bVisible);
	void SyncShownPageToPlayerRegion();
	void StartMapUpdateTimers();
	void StopMapUpdateTimers();
	void RefreshAreaMapRegionMarkers();
	void UpdateCharacterMark();
	void UpdateTeamMarks();
	void HideDesignerMarkerWidgets() const;
	FMapMarkerCanvas MakeMarkerCanvas();
	UPanelWidget* GetMarkerParentPanel() const;
	FMapAreaRegionBoxes GetAreaRegionBoxes() const;
	int32 FindPlayerPageIndex(const APawn& Pawn) const;
	bool DoesPawnMatchShownPage(const APawn& Pawn) const;
	FVector2D GetShownViewSize() const;
	FText GetShownMapName() const;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "!UI|Map", meta = (BindWidgetOptional))
	TObjectPtr<UButton> WindmillButton;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Map", meta = (BindWidgetOptional))
	TObjectPtr<UButton> DomeButton;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Map", meta = (BindWidgetOptional))
	TObjectPtr<UButton> TempleButton;

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

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Map|Marker")
	bool bShowRemotePlayerMarks = true;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Map|Marker", meta = (ClampMin = "0.02", ForceUnits = "s"))
	float MarkerUpdateInterval = 0.1f;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Map|Marker", meta = (ClampMin = "0.1", ForceUnits = "s"))
	float RemotePlayerListRefreshInterval = 0.5f;

private:
	FLevelMapLayout Layout;
	FText LevelName;
	bool bRotateMarksToPawnForward = true;
	float MarkerRotationOffsetDegrees = 0.0f;
	FVector2D MaxMapViewSize = FVector2D(1200.0f, 900.0f);

	UPROPERTY(Transient)
	FMapPawnMarkers PawnMarkers;

	FMapMarkImages MarkImages;
	FMapAreaRegionMarkers AreaRegionMarkers;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<APawn>> CachedRemotePlayerPawns;

	/** 지역 단추마다 하나: 누르면 그 지역 지도로, 마우스를 올리면 그 지역 빛을 켠다. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UButtonClickRelay>> RegionButtonRelays;

	/** 레벨 지도의 Pages 번호마다 전체 지도 위에 덧그리는 지역 빛(없으면 빈 칸). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> RegionHighlights;

	FTimerHandle MarkerUpdateTimerHandle;
	FTimerHandle RemotePlayerListRefreshTimerHandle;

	int32 ShownPageIndex = INDEX_NONE;
	int32 LastSyncedPlayerPageIndex = INDEX_NONE;
	bool bHasSyncedPlayerPage = false;
	bool bHasManualPageSelection = false;
};
