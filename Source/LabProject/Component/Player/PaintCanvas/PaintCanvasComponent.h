#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Component/Player/PaintCanvas/PaintCanvasSync.h"
#include "Iris/ReplicationState/IrisFastArraySerializer.h"
#include "TimerManager.h"
#include "PaintCanvasComponent.generated.h"

class APdPlayer;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPaintCanvasDisplay;
class UPrimitiveComponent;
class UTexture2D;
class UTextureRenderTarget2D;
class UPaintCanvasComponent;

USTRUCT()
struct LABPROJECT_API FPaintCanvasStrokeRequest
{
    GENERATED_BODY()

    UPROPERTY()
    float BrushSize = 0.0f;

    UPROPERTY()
    FVector2f DrawLocation = FVector2f::ZeroVector;
};

USTRUCT()
struct LABPROJECT_API FReplicatedPaintCanvasStroke : public FFastArraySerializerItem
{
    GENERATED_BODY()

    UPROPERTY()
    uint32 Sequence = 0;

    UPROPERTY()
    float BrushSize = 0.0f;

    UPROPERTY()
    FVector2f DrawLocation = FVector2f::ZeroVector;
};


USTRUCT()
struct LABPROJECT_API FReplicatedPaintCanvasStrokeArray : public FIrisFastArraySerializer
{
    GENERATED_BODY()

    UPROPERTY()
    TArray<FReplicatedPaintCanvasStroke> Items;

    bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
    {
        return FFastArraySerializer::FastArrayDeltaSerialize<
            FReplicatedPaintCanvasStroke,
            FReplicatedPaintCanvasStrokeArray>(
                Items,
                DeltaParams,
                *this);
    }

    void PostReplicatedAdd(const TArrayView<int32>&, int32);

    void PostReplicatedChange(const TArrayView<int32>&, int32);

    void PreReplicatedRemove(const TArrayView<int32>&, int32);

    void SetOwner(UPaintCanvasComponent* InOwner)
    {
        Owner = InOwner;
    }

private:
    UPaintCanvasComponent* Owner = nullptr;
};

template <>
struct TStructOpsTypeTraits<FReplicatedPaintCanvasStrokeArray>
    : public TStructOpsTypeTraitsBase2<FReplicatedPaintCanvasStrokeArray>
{
    enum
    {
        WithNetDeltaSerializer = true,
    };
};

USTRUCT()
struct LABPROJECT_API FReplicatedPaintCanvasStateHeader
{
    GENERATED_BODY()

    UPROPERTY()
    uint32 Revision = 1;

    UPROPERTY()
    uint32 LastSequence = 0;

    UPROPERTY()
    uint32 Checksum = 0;
};

/**
 * Paint 기능의 외부 진입점과 네트워크 흐름을 조율한다.
 *
 * - RenderTarget에 Stroke를 그리는 일은 이 컴포넌트가 직접 담당한다.
 * - Sequence/Revision/Checksum 규칙은 PaintCanvasSync로 분리한다.
 * - Speech Bubble/Face Decal 월드 표현은 UPaintCanvasPresentation에 위임한다.
 */
UCLASS(BlueprintType, Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LABPROJECT_API UPaintCanvasComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPaintCanvasComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    void SetSpeechBubbleComponent(UPrimitiveComponent* InSpeechBubbleComponent);

    bool BeginPaintCanvasUiSession();
    bool PaintAtNormalizedLocation(const FVector2D& DrawLocation);
    void HidePaintCanvas();
    bool HasActivePaintCanvas() const;

    UTextureRenderTarget2D* GetActivePaintCanvasRenderTarget() const;

    bool ExportActivePaintCanvasToSpeechBubble();

    bool ApplyActivePaintCanvasToFaceDecal(
        UMaterialInterface* FaceDecalMaterial,
        FName AttachSocketName,
        const FTransform& FaceDecalTransformOffset,
        FVector FaceDecalSize,
        FName TextureParameterName);

    void RestoreCachedLobbyPaintCanvasFaceDecal();
    void HidePaintSpeechBubble();

private:
    friend struct FReplicatedPaintCanvasStrokeArray;

    APdPlayer* GetPlayerOwner() const;
    UPaintCanvasDisplay* GetOrCreatePresentation();

    // Canvas rendering
    bool EnsurePaintCanvasRenderResources(bool bResetCanvas = false);
    void ResetPaintCanvasRenderTarget();
    bool DrawBrushToRenderTarget(
        UTexture2D* InBrushTexture,
        double InBrushSize,
        const FVector2D& DrawLocation);
    void ApplyBrushMaterialParameters() const;
    UTextureRenderTarget2D* CreatePaintCanvasCopy(
        UTextureRenderTarget2D* SourceRenderTarget,
        double Scale,
        const FLinearColor& ClearTargetColor);
    void RebuildCanvasFromAuthoritativeHistory(const TArray<const FReplicatedPaintCanvasStroke*>& SortedStrokes);

    // Local session / presentation orchestration
    void CancelPaintCanvasExport();
    void HandlePaintCanvasExportExpired();
    bool ApplyPaintCanvasToSpeechBubble();
    bool StartLocalPaintCanvasExport();
    bool ApplyLocalPaintCanvasToFaceDecal(
        UMaterialInterface* FaceDecalMaterial,
        FName AttachSocketName,
        const FTransform& FaceDecalTransformOffset,
        FVector FaceDecalSize,
        FName TextureParameterName);
    void ProcessPendingPaintPresentation();

    // Network / authoritative history
    void ResetLocalSharedPaintCanvas();
    void SubmitPaintCanvasResetForNetwork();
    void SubmitPaintCanvasStrokeForNetwork(
        double InBrushSize,
        const FVector2D& DrawLocation);
    void FlushPendingPaintStrokeBatches(bool bFlushAll);
    void SchedulePendingPaintStrokeFlush();
    void CommitAuthoritativePaintStrokeBatch(
        const TArray<FPaintCanvasStrokeRequest>& StrokeBatch);
    void ResetAuthoritativePaintCanvas();
    void SchedulePaintStateReconciliation();
    void ReconcileReplicatedPaintState();
    void SubmitPaintCanvasExportForNetwork();

    void SubmitPaintCanvasFaceDecalForNetwork(
        UMaterialInterface* FaceDecalMaterial,
        FName AttachSocketName,
        const FTransform& FaceDecalTransformOffset,
        FVector FaceDecalSize,
        FName TextureParameterName);

    bool TryConsumePaintNetworkEvent(
        double& LastAcceptedTime,
        double MinInterval);

    // Travel bridge. Lobby 의존성은 별도 리팩터링에서 역전하는 편이 안전하다.
    void CacheLocalPaintCanvasStrokeForTravel(
        UTexture2D* InBrushTexture,
        double InBrushSize,
        const FVector2D& DrawLocation) const;
    void CacheLocalPaintCanvasFaceDecalForTravel(
        UMaterialInterface* FaceDecalMaterial,
        FName AttachSocketName,
        const FTransform& FaceDecalTransformOffset,
        FVector FaceDecalSize,
        FName TextureParameterName) const;

    UFUNCTION(Server, Reliable)
    void ServerResetSharedPaintCanvas();

    UFUNCTION(Server, Reliable)
    void ServerSubmitPaintCanvasStrokeBatch(const TArray<FPaintCanvasStrokeRequest>& StrokeBatch);

    UFUNCTION(Server, Reliable)
    void ServerExportPaintCanvas();

    UFUNCTION(NetMulticast, Reliable)
    void MulticastExportPaintCanvas(
        uint32 RequiredRevision,
        uint32 RequiredLastSequence,
        uint32 RequiredChecksum);

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
        FName TextureParameterName,
        uint32 RequiredRevision,
        uint32 RequiredLastSequence,
        uint32 RequiredChecksum);

    UFUNCTION()
    void OnRep_PaintCanvasStateHeader();

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

    UPROPERTY(EditDefaultsOnly, Category = "!Paint|Network", meta = (ClampMin = "1", ClampMax = "64"))
    int32 MaxPaintStrokesPerBatch = 32;

    UPROPERTY(EditDefaultsOnly, Category = "!Paint|Network", meta = (ClampMin = "0.005", ForceUnits = "s"))
    double PaintStrokeBatchInterval = 0.02;

    UPROPERTY(EditDefaultsOnly, Category = "!Paint|Network", meta = (ClampMin = "1", ClampMax = "8192"))
    int32 MaxReplicatedPaintStrokeHistory = 4096;

    UPROPERTY(EditDefaultsOnly, Category = "!Paint|Network", meta = (ClampMin = "0.0", ForceUnits = "s"))
    double PaintControlNetworkMinInterval = 0.15;

    UPROPERTY(EditDefaultsOnly, Category = "!Paint|Network", meta = (ClampMin = "1.0"))
    double MaxReplicatedPaintBrushSize = 256.0;

    UPROPERTY(EditDefaultsOnly, Category = "!Paint|Network", meta = (ClampMin = "0.01"))
    double MaxReplicatedPaintTransformScale = 4.0;

    UPROPERTY(EditDefaultsOnly, Category = "!Paint|Network", meta = (ClampMin = "1.0", ForceUnits = "cm"))
    double MaxReplicatedFaceDecalSize = 500.0;

    UPROPERTY(EditDefaultsOnly, Category = "!Paint|Network", meta = (ClampMin = "0.0", ForceUnits = "cm"))
    double MaxReplicatedFaceDecalOffsetDistance = 500.0;

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

    // FastArray 사용
    UPROPERTY(Replicated)
    FReplicatedPaintCanvasStrokeArray ReplicatedPaintStrokes;

    UPROPERTY(ReplicatedUsing = OnRep_PaintCanvasStateHeader)
    FReplicatedPaintCanvasStateHeader ReplicatedPaintStateHeader;

    TArray<FPaintCanvasStrokeRequest> PendingPaintStrokeRequests;
    FTimerHandle PaintStrokeBatchTimerHandle;
    FTimerHandle PaintStateReconcileTimerHandle;

    FPaintCanvasLocalSyncState LocalSyncState;

    bool bHasPendingPaintExport = false;
    uint32 PendingPaintExportRevision = 0;
    uint32 PendingPaintExportLastSequence = 0;
    uint32 PendingPaintExportChecksum = 0;

    bool bHasPendingFaceDecal = false;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInterface> PendingFaceDecalMaterial = nullptr;

    FName PendingFaceDecalAttachSocketName = NAME_None;
    FTransform PendingFaceDecalTransformOffset = FTransform::Identity;
    FVector PendingFaceDecalSize = FVector::ZeroVector;
    FName PendingFaceDecalTextureParameterName = NAME_None;
    uint32 PendingFaceDecalRevision = 0;
    uint32 PendingFaceDecalLastSequence = 0;
    uint32 PendingFaceDecalChecksum = 0;

    double LastPaintExportServerTime = -1.0e30;
    double LastPaintFaceDecalServerTime = -1.0e30;
};
