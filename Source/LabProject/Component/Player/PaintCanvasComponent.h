#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "TimerManager.h"
#include "PaintCanvasComponent.generated.h"

class AActor;
class APdPlayer;
class UDecalComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPrimitiveComponent;
class UTexture2D;
class UTextureRenderTarget2D;

UCLASS(BlueprintType, Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LABPROJECT_API UPaintCanvasComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPaintCanvasComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void SetSpeechBubbleComponent(UPrimitiveComponent* InSpeechBubbleComponent);

	bool TryPaintAtCursor();

	AActor* ShowPaintCanvasWithCharacterOffset(const FTransform& PaintCanvasTransformOffset);

	void HidePaintCanvas();

	bool HasActivePaintCanvas() const;

	bool ExportActivePaintCanvasAboveCharacterWithTransformOffset(const FTransform& PaintCanvasExportTransformOffset);

	bool ApplyActivePaintCanvasToFaceDecal(
		UMaterialInterface* FaceDecalMaterial,
		FName AttachSocketName,
		const FTransform& FaceDecalTransformOffset,
		FVector FaceDecalSize,
		FName TextureParameterName);

	void RestoreCachedLobbyPaintCanvasFaceDecal();
	void HidePaintSpeechBubble();

private:
	APdPlayer* GetPlayerOwner() const;
	void CenterPaintCanvasVisualOnView(AActor* PaintCanvasActor, const FVector& DesiredVisualCenter) const;
	void ConfigurePaintCanvasCollision(AActor* PaintCanvasActor) const;
	void CancelPaintCanvasExport();
	void FinishPaintCanvasExport();
	UPrimitiveComponent* FindPaintSpeechBubbleComponent() const;
	UTextureRenderTarget2D* CreateScaledPaintCanvasRenderTarget(UTextureRenderTarget2D* SourceRenderTarget);
	bool ApplyPaintCanvasToSpeechBubble();
	bool PaintAtHitResult(const FHitResult& HitResult);
	bool TryGetDrawLocationFromHitResult(const FHitResult& HitResult, FVector2D& OutDrawLocation) const;
	bool DispatchDrawBrush(AActor* HitActor, UTexture2D* InBrushTexture, double InBrushSize, const FVector2D& DrawLocation) const;
	UTextureRenderTarget2D* GetActivePaintCanvasRenderTarget() const;
	UTextureRenderTarget2D* CreatePaintCanvasFaceDecalSnapshot(UTextureRenderTarget2D* SourceRenderTarget);
	bool EnsurePaintCanvasActorForSharedDisplay();
	bool StartLocalPaintCanvasExportAboveCharacter(const FTransform& PaintCanvasExportTransformOffset);
	void ResetLocalSharedPaintCanvas();
	void SubmitPaintCanvasResetForNetwork();
	void SubmitPaintCanvasStrokeForNetwork(UTexture2D* InBrushTexture, double InBrushSize, const FVector2D& DrawLocation);
	void SubmitPaintCanvasExportForNetwork(const FTransform& PaintCanvasExportTransformOffset);
	void ClearPaintCanvasFaceDecal();
	void CacheLocalPaintCanvasStrokeForTravel(UTexture2D* InBrushTexture, double InBrushSize, const FVector2D& DrawLocation) const;
	void CacheLocalPaintCanvasFaceDecalForTravel(
		UMaterialInterface* FaceDecalMaterial,
		FName AttachSocketName,
		const FTransform& FaceDecalTransformOffset,
		FVector FaceDecalSize,
		FName TextureParameterName) const;
	bool ApplyLocalPaintCanvasToFaceDecal(
		UMaterialInterface* FaceDecalMaterial,
		FName AttachSocketName,
		const FTransform& FaceDecalTransformOffset,
		FVector FaceDecalSize,
		FName TextureParameterName);
	void SubmitPaintCanvasFaceDecalForNetwork(
		UMaterialInterface* FaceDecalMaterial,
		FName AttachSocketName,
		const FTransform& FaceDecalTransformOffset,
		FVector FaceDecalSize,
		FName TextureParameterName);
	bool TryConsumePaintNetworkEvent(double& LastAcceptedTime, double MinInterval);
	double GetClampedReplicatedPaintBrushSize(double InBrushSize) const;
	bool IsValidReplicatedDrawLocation(const FVector2D& DrawLocation) const;
	bool IsValidReplicatedPaintTransform(const FTransform& Transform, double MaxTranslationDistance, double MaxScale) const;
	bool IsValidReplicatedFaceDecalPayload(
		UMaterialInterface* FaceDecalMaterial,
		const FTransform& FaceDecalTransformOffset,
		const FVector& FaceDecalSize) const;

	UFUNCTION(Server, Reliable)
	void ServerResetSharedPaintCanvas();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastResetSharedPaintCanvas();

	UFUNCTION(Server, Unreliable)
	void ServerSubmitPaintCanvasStroke(UTexture2D* InBrushTexture, double InBrushSize, FVector2D DrawLocation);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastSubmitPaintCanvasStroke(UTexture2D* InBrushTexture, double InBrushSize, FVector2D DrawLocation);

	UFUNCTION(Server, Reliable)
	void ServerExportPaintCanvas(FTransform PaintCanvasExportTransformOffset);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastExportPaintCanvas(FTransform PaintCanvasExportTransformOffset);

	UFUNCTION(Server, Reliable)
	void ServerApplyPaintCanvasFaceDecal(
		UMaterialInterface* FaceDecalMaterial,
		FName AttachSocketName,
		FTransform FaceDecalTransformOffset,
		FVector FaceDecalSize,
		FName TextureParameterName);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastApplyPaintCanvasFaceDecal(
		UMaterialInterface* FaceDecalMaterial,
		FName AttachSocketName,
		FTransform FaceDecalTransformOffset,
		FVector FaceDecalSize,
		FName TextureParameterName);

private:
	UPROPERTY(EditAnywhere, Category = "!Paint", meta = (DisplayName = "Brush Texture"))
	TObjectPtr<UTexture2D> BrushTexture;

	UPROPERTY(EditAnywhere, Category = "!Paint", meta = (DisplayName = "Brush Size", ClampMin = "0.0"))
	double BrushSize = 64.0;

	UPROPERTY(EditAnywhere, Category = "!Paint", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	double PaintTraceDistance = 1500.0;

	UPROPERTY(EditAnywhere, Category = "!Paint")
	TEnumAsByte<ETraceTypeQuery> PaintTraceChannel = TraceTypeQuery1;

	UPROPERTY(EditAnywhere, Category = "!Paint")
	TSubclassOf<AActor> PaintCanvasActorClass;

	UPROPERTY(EditDefaultsOnly, Category = "!Paint|Network", meta = (ClampMin = "0.0", ForceUnits = "s"))
	double PaintStrokeNetworkMinInterval = 0.02;

	UPROPERTY(EditDefaultsOnly, Category = "!Paint|Network", meta = (ClampMin = "0.0", ForceUnits = "s"))
	double PaintControlNetworkMinInterval = 0.15;

	UPROPERTY(EditDefaultsOnly, Category = "!Paint|Network", meta = (ClampMin = "1.0"))
	double MaxReplicatedPaintBrushSize = 256.0;

	UPROPERTY(EditDefaultsOnly, Category = "!Paint|Network", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	double MaxReplicatedPaintExportOffsetDistance = 600.0;

	UPROPERTY(EditDefaultsOnly, Category = "!Paint|Network", meta = (ClampMin = "0.01"))
	double MaxReplicatedPaintTransformScale = 4.0;

	UPROPERTY(EditDefaultsOnly, Category = "!Paint|Network", meta = (ClampMin = "1.0", ForceUnits = "cm"))
	double MaxReplicatedFaceDecalSize = 500.0;

	UPROPERTY(EditDefaultsOnly, Category = "!Paint|Network", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	double MaxReplicatedFaceDecalOffsetDistance = 500.0;

	UPROPERTY(Transient)
	TObjectPtr<AActor> ActivePaintCanvasActor;

	UPROPERTY(EditAnywhere, Category = "!Paint|Export", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	FVector PaintCanvasExportOffset = FVector(0.0, 0.0, 260.0);

	UPROPERTY(EditAnywhere, Category = "!Paint|Export", meta = (ClampMin = "0.1", ForceUnits = "s"))
	double PaintCanvasExportDuration = 10.0;

	UPROPERTY(EditAnywhere, Category = "!Paint|Export")
	FName PaintSpeechBubbleComponentName = TEXT("SpeechBubble");

	UPROPERTY(EditAnywhere, Category = "!Paint|Export")
	FName PaintSpeechBubbleRenderTargetParameterName = TEXT("RenderTarget");

	UPROPERTY(EditAnywhere, Category = "!Paint|Export", meta = (ClampMin = "0"))
	int32 PaintSpeechBubbleMaterialIndex = 0;

	UPROPERTY(EditAnywhere, Category = "!Paint|Export", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	double PaintSpeechBubbleRenderTargetScale = 0.75;

	UPROPERTY(Transient)
	bool bPaintCanvasExportActive = false;

	FTimerHandle PaintCanvasExportTimerHandle;

	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> SpeechBubbleComponent;

	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> ActivePaintSpeechBubbleComponent;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ActivePaintSpeechBubbleMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> ActivePaintSpeechBubbleRenderTarget;

	UPROPERTY(Transient)
	TObjectPtr<UDecalComponent> ActivePaintCanvasFaceDecalComponent;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ActivePaintCanvasFaceDecalMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> ActivePaintCanvasFaceDecalSnapshot;

	double LastPaintResetServerTime = -1.0e30;
	double LastPaintStrokeServerTime = -1.0e30;
	double LastPaintExportServerTime = -1.0e30;
	double LastPaintFaceDecalServerTime = -1.0e30;
};
