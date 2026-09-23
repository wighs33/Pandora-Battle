#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;
struct FReplicatedPaintCanvasStateHeader;
struct FReplicatedPaintCanvasStroke;

/**
 * 로컬에 적용된 Paint 상태와 로컬 예측 상태만 보관한다.
 * 복제 자체는 UPaintCanvasComponent가 담당하고, 이 구조체는 상태 비교에만 사용한다.
 */
struct LABPROJECT_API FPaintCanvasLocalSyncState
{
    uint32 AppliedRevision = 0;
    uint32 AppliedLastSequence = 0;
    uint32 AppliedChecksum = 0;

    int32 PredictedStrokeCount = 0;
    uint32 PredictedChecksum = 0;

    void Reset();
    void ResetAppliedToAuthoritative(const FReplicatedPaintCanvasStateHeader& Header);
    bool IsAppliedStateSynchronized(uint32 Revision, uint32 LastSequence, uint32 Checksum) const;
    void AcceptAuthoritative(const FReplicatedPaintCanvasStateHeader& Header, uint32 ValidatedChecksum);
};

/**
 * Paint 네트워크 상태에 대한 순수 규칙 모음.
 * World, Actor, RPC, RenderTarget을 알지 않으므로 독립적으로 테스트할 수 있다.
 */
namespace PaintCanvasSync
{
    LABPROJECT_API double ClampBrushSize(double BrushSize, double MaxBrushSize);
    LABPROJECT_API bool IsValidDrawLocation(const FVector2D& DrawLocation);
    LABPROJECT_API bool IsValidTransform(
        const FTransform& Transform,
        double MaxTranslationDistance,
        double MaxScale);
    LABPROJECT_API bool IsValidFaceDecalPayload(
        const UMaterialInterface* FaceDecalMaterial,
        const FTransform& FaceDecalTransformOffset,
        const FVector& FaceDecalSize,
        double MaxFaceDecalSize,
        double MaxTranslationDistance,
        double MaxScale);

    LABPROJECT_API uint32 AccumulateStrokeChecksum(
        uint32 CurrentChecksum,
        const FReplicatedPaintCanvasStroke& Stroke);

    /**
     * Stroke 포인터 목록을 Sequence 순서로 정렬한 뒤 연속성과 Checksum을 검증한다.
     * 실패하면 목록과 Checksum을 비워 반환한다.
     */
    LABPROJECT_API bool ValidateAuthoritativeHistory(
        TArray<const FReplicatedPaintCanvasStroke*>& InOutSortedStrokes,
        const FReplicatedPaintCanvasStateHeader& Header,
        int32 MaxHistoryCount,
        uint32& OutChecksum);
}
