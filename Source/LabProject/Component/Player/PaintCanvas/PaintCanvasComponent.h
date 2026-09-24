#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "PaintCanvasComponent.generated.h"

class APdPlayer;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPaintCanvasDisplay;
class UPrimitiveComponent;
class UTexture2D;
class UTextureRenderTarget2D;

USTRUCT()
struct LABPROJECT_API FPaintCanvasStroke
{
    GENERATED_BODY()

    UPROPERTY()
    float BrushSize = 0.0f;

    UPROPERTY()
    FVector2f DrawLocation = FVector2f::ZeroVector;

    UPROPERTY()
    bool bStartsNewStroke = true;
};

UCLASS(BlueprintType, Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LABPROJECT_API UPaintCanvasComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    // Engine Overrides ------------------------------------------------------------------------------------------------
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // Public API ------------------------------------------------------------------------------------------------------
    UPaintCanvasComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    void SetSpeechBubbleComponent(UPrimitiveComponent* InSpeechBubbleComponent);

    bool BeginPaintCanvasUiSession();
    bool PaintAtNormalizedLocation(const FVector2D& DrawLocation);
    void ResetPaintStroke();
    void HidePaintCanvas();
    bool HasActivePaintCanvas() const;

    UTextureRenderTarget2D* GetActivePaintCanvasRenderTarget() const;

    bool ExportActivePaintCanvasToSpeechBubble();

    bool ApplyActivePaintCanvasToFaceDecal(UMaterialInterface* FaceDecalMaterial, FName AttachSocketName, const FTransform& FaceDecalTransformOffset, FVector FaceDecalSize, FName TextureParameterName);

    void RestoreCachedLobbyPaintCanvasFaceDecal();

private:
    // Network RPCs ----------------------------------------------------------------------------------------------------
    UFUNCTION(Server, Reliable)
    void ServerExportPaintCanvas(const TArray<FPaintCanvasStroke>& Strokes);

    UFUNCTION(NetMulticast, Reliable)
    void MulticastExportPaintCanvas(const TArray<FPaintCanvasStroke>& Strokes);

    UFUNCTION(Server, Reliable)
    void ServerApplyPaintCanvasFaceDecal(const TArray<FPaintCanvasStroke>& Strokes, UMaterialInterface* FaceDecalMaterial, FName AttachSocketName, FTransform FaceDecalTransformOffset, FVector FaceDecalSize, FName TextureParameterName);

    UFUNCTION(NetMulticast, Reliable)
    void MulticastApplyPaintCanvasFaceDecal(const TArray<FPaintCanvasStroke>& Strokes, UMaterialInterface* FaceDecalMaterial, FName AttachSocketName, FTransform FaceDecalTransformOffset, FVector FaceDecalSize, FName TextureParameterName);

    // Event Handlers --------------------------------------------------------------------------------------------------
    void HandlePaintCanvasExportExpired();

    // Internal Helpers ------------------------------------------------------------------------------------------------
    APdPlayer* GetPlayerOwner() const;
    UPaintCanvasDisplay* GetOrCreatePresentation();

    bool EnsurePaintCanvasRenderResources(bool bResetCanvas = false);
    void ResetPaintCanvasRenderTarget();
    bool DrawBrushToRenderTarget(UTexture2D* InBrushTexture, double InBrushSize, const FVector2D& DrawLocation);
    bool DrawPaintStroke(const FPaintCanvasStroke& Stroke, const FPaintCanvasStroke* PreviousStroke);
    UTextureRenderTarget2D* CreatePaintCanvasCopy(UTextureRenderTarget2D* SourceRenderTarget, double Scale, const FLinearColor& ClearTargetColor);
    bool ReplayPaintCanvasStrokes(const TArray<FPaintCanvasStroke>& Strokes);

    void CancelPaintCanvasExport();
    void HidePaintSpeechBubble();
    bool ApplyPaintCanvasToSpeechBubble();
    bool StartLocalPaintCanvasExport();
    bool ApplyLocalPaintCanvasToFaceDecal(UMaterialInterface* FaceDecalMaterial, FName AttachSocketName, const FTransform& FaceDecalTransformOffset, FVector FaceDecalSize, FName TextureParameterName);

    bool IsValidPaintCanvasStrokes(const TArray<FPaintCanvasStroke>& Strokes) const;

    bool TryConsumePaintNetworkEvent(double& LastAcceptedTime, double MinInterval);

    void CacheLocalPaintCanvasFaceDecalForTravel(UMaterialInterface* FaceDecalMaterial, FName AttachSocketName, const FTransform& FaceDecalTransformOffset, FVector FaceDecalSize, FName TextureParameterName) const;

private:
    UPROPERTY(EditAnywhere, Category = "!Paint", meta = (DisplayName = "Brush Texture"))
    TObjectPtr<UTexture2D> BrushTexture;

    UPROPERTY(EditAnywhere, Category = "!Paint", meta = (DisplayName = "Brush Size", ClampMin = "0.0"))
    double BrushSize = 64.0;

    UPROPERTY(EditDefaultsOnly, Category = "!Paint|Render Target", meta = (ClampMin = "1"))
    int32 RenderTargetWidth = 1024;

    UPROPERTY(EditDefaultsOnly, Category = "!Paint|Render Target", meta = (ClampMin = "1"))
    int32 RenderTargetHeight = 1024;

    UPROPERTY(EditDefaultsOnly, Category = "!Paint|Render Target")
    FLinearColor ClearColor = FLinearColor::White;

    UPROPERTY(EditDefaultsOnly, Category = "!Paint|Material")
    FName BrushTextureParameterName = TEXT("BrushTexture");

    UPROPERTY(EditDefaultsOnly, Category = "!Paint|Material")
    FName BrushColorParameterName = TEXT("BrushColor");

    UPROPERTY(EditDefaultsOnly, Category = "!Paint|Network", meta = (ClampMin = "1", ClampMax = "8192"))
    int32 MaxReplicatedPaintStrokeHistory = 4096;

    UPROPERTY(EditDefaultsOnly, Category = "!Paint|Network", meta = (ClampMin = "0.0", ForceUnits = "s"))
    double PaintControlNetworkMinInterval = 0.15;

    UPROPERTY(EditDefaultsOnly, Category = "!Paint|Network", meta = (ClampMin = "1.0"))
    double MaxReplicatedPaintBrushSize = 256.0;

    UPROPERTY(Transient)
    TObjectPtr<UTextureRenderTarget2D> PaintCanvasRenderTarget;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> PaintBrushMaterial;

    UPROPERTY(Transient)
    TObjectPtr<UPaintCanvasDisplay> Presentation;

    UPROPERTY(Transient)
    bool bPaintCanvasUiSessionActive = false;

    UPROPERTY(EditAnywhere, Category = "!Paint|Export", meta = (ClampMin = "0.1", ForceUnits = "s"))
    double PaintCanvasExportDuration = 10.0;

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
    TArray<FPaintCanvasStroke> LocalPaintStrokes;

    bool bHasPreviousPaintLocation = false;

    double LastPaintExportServerTime = -1.0e30;
    double LastPaintFaceDecalServerTime = -1.0e30;
};
