#pragma once

#include "Blueprint/UserWidget.h"
#include "Definition/UI/WidgetClassDefinition.h"
#include "MapWidget.generated.h"

class UButton;
class UCanvasPanel;
class UImage;
class UPanelWidget;
class USizeBox;
class UTextBlock;
class UTexture2D;
class APawn;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UMapWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UMapWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	enum class EMapView : uint8
	{
		Area,
		Windmill,
		Dome,
		Temple
	};

	UFUNCTION(BlueprintCallable, Category = "!UI|Map")
	void ShowAreaMap();

	UFUNCTION(BlueprintCallable, Category = "!UI|Map")
	void ShowWindmillMap();

	UFUNCTION(BlueprintCallable, Category = "!UI|Map")
	void ShowDomeMap();

	UFUNCTION(BlueprintCallable, Category = "!UI|Map")
	void ShowTempleMap();

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

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

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Map|Size")
	FVector2D AreaMapTotalSizeBoxSize = FVector2D(900.0f, 900.0f);

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Map|Size")
	FVector2D WindmillMapTotalSizeBoxSize = FVector2D(900.0f, 900.0f);

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Map|Size")
	FVector2D DomeMapTotalSizeBoxSize = FVector2D(900.0f, 900.0f);

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Map|Size")
	FVector2D TempleMapTotalSizeBoxSize = FVector2D(900.0f, 900.0f);

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Map|Plane")
	FTransform WindmillPlaneTransform = FTransform::Identity;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Map|Plane")
	FTransform TemplePlaneTransform = FTransform::Identity;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Map|Plane")
	FTransform DomePlaneTransform = FTransform::Identity;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Map|Marker")
	bool bShowRemotePlayerMarks = true;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Map|Marker", meta = (ClampMin = "0.02", ForceUnits = "s"))
	float MarkerUpdateInterval = 0.1f;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Map|Marker", meta = (ClampMin = "0.1", ForceUnits = "s"))
	float RemotePlayerListRefreshInterval = 0.5f;

private:
	UFUNCTION()
	void OnWindmillButtonClicked();

	UFUNCTION()
	void OnDomeButtonClicked();

	UFUNCTION()
	void OnTempleButtonClicked();

	UFUNCTION()
	void OnAreaMapButtonClicked();

	void ApplyMapView(EMapView NewMapView);
	void ApplyMapTexture(UTexture2D* Texture);
	void ApplyTotalSizeBoxSize(const FVector2D& Size);
	void ResolveDefaultTextures();
	void ApplyWidgetDefinitionSettings();
	void ApplyProjectionSettings(const FMapWidgetProjectionSettings& ProjectionSettings);
	bool ApplyMarkImage(UImage* MarkWidget, UObject* ResourceObject, const FVector2D& DesiredImageSize) const;
	const FMapWidgetProjectionSettings* FindProjectionOverride(const FMapWidgetSettings& Settings) const;
	void SetRegionSelectionButtonsVisible(bool bVisible);
	void SyncMapViewToPlayerMapRegion();
	void StartMapUpdateTimers();
	void StopMapUpdateTimers();
	void HandleMapUpdateTick();
	void RefreshRemotePlayerPawns();
	void UpdateCharacterMark();
	void UpdateTeamMarks();
	void HideTeamMarks();
	void HideSelfMarks();
	void HideDesignerMarkerWidgets() const;
	void EnsureTeamMarkCapacity(int32 RequiredCount);
	UImage* CreateDynamicMarkerWidget(FName MarkerName, int32 ZOrder);
	UPanelWidget* GetMarkerParentPanel() const;
	bool UpdatePawnMapMark(UImage* MarkWidget, const APawn& Pawn, bool bRotateToPawnForward = true) const;
	bool ApplyCharacterMarkImage(UImage* MarkWidget) const;
	bool ApplyTeamMarkImage(UImage* MarkWidget, const APawn& Pawn) const;
	bool DoesPawnMatchCurrentMapView(const APawn& Pawn) const;
	const FTransform& GetCurrentPlaneTransform() const;
	FVector2D GetCurrentTotalSizeBoxSize() const;

	UPROPERTY(Transient)
	TObjectPtr<UImage> SelfTeamMarkWidget;

	UPROPERTY(Transient)
	TObjectPtr<UImage> SelfCharacterMarkWidget;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> RemoteTeamMarkWidgets;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<APawn>> CachedRemotePlayerPawns;

	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UObject>> TeamMarkImagesByTeamColorIndex;

	UPROPERTY(Transient)
	TMap<int32, FVector2D> TeamMarkImageSizesByTeamColorIndex;

	UPROPERTY(Transient)
	TObjectPtr<UObject> CharacterMarkResourceObject;

	UPROPERTY(Transient)
	FVector2D CharacterMarkResolvedImageSize = FVector2D::ZeroVector;

	UPROPERTY(Transient)
	float PlaneLocalSize = 100.0f;

	UPROPERTY(Transient)
	bool bRotateMarksToPawnForward = true;

	UPROPERTY(Transient)
	float MarkerRotationOffsetDegrees = 0.0f;

	FTimerHandle MarkerUpdateTimerHandle;
	FTimerHandle RemotePlayerListRefreshTimerHandle;

	EMapView CurrentMapView = EMapView::Windmill;
	EMapView LastSyncedPlayerMapView = EMapView::Windmill;
	bool bHasSyncedPlayerMapView = false;
	bool bHasManualMapViewSelection = false;
};
