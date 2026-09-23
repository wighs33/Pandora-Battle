#include "Component/Player/PaintCanvas/PaintCanvasSync.h"

#include "Component/Player/PaintCanvas/PaintCanvasComponent.h"
#include "Materials/MaterialInterface.h"

void FPaintCanvasLocalSyncState::Reset()
{
    AppliedRevision = 0;
    AppliedLastSequence = 0;
    AppliedChecksum = 0;
    PredictedStrokeCount = 0;
    PredictedChecksum = 0;
}

void FPaintCanvasLocalSyncState::ResetAppliedToAuthoritative(
    const FReplicatedPaintCanvasStateHeader& Header)
{
    AppliedRevision = Header.Revision;
    AppliedLastSequence = Header.LastSequence;
    AppliedChecksum = Header.Checksum;
    PredictedStrokeCount = static_cast<int32>(Header.LastSequence);
    PredictedChecksum = Header.Checksum;
}

bool FPaintCanvasLocalSyncState::IsAppliedStateSynchronized(
    const uint32 Revision,
    const uint32 LastSequence,
    const uint32 Checksum) const
{
    return AppliedRevision == Revision
        && AppliedLastSequence == LastSequence
        && AppliedChecksum == Checksum;
}

void FPaintCanvasLocalSyncState::AcceptAuthoritative(
    const FReplicatedPaintCanvasStateHeader& Header,
    const uint32 ValidatedChecksum)
{
    AppliedRevision = Header.Revision;
    AppliedLastSequence = Header.LastSequence;
    AppliedChecksum = ValidatedChecksum;
    PredictedStrokeCount = static_cast<int32>(Header.LastSequence);
    PredictedChecksum = ValidatedChecksum;
}

double PaintCanvasSync::ClampBrushSize(
    const double BrushSize,
    const double MaxBrushSize)
{
    if (!FMath::IsFinite(BrushSize) || BrushSize <= 0.0)
    {
        return 0.0;
    }

    return FMath::Clamp(
        BrushSize,
        1.0,
        FMath::Max(MaxBrushSize, 1.0));
}

bool PaintCanvasSync::IsValidDrawLocation(const FVector2D& DrawLocation)
{
    return FMath::IsFinite(DrawLocation.X)
        && FMath::IsFinite(DrawLocation.Y)
        && DrawLocation.X >= 0.0
        && DrawLocation.X <= 1.0
        && DrawLocation.Y >= 0.0
        && DrawLocation.Y <= 1.0;
}

bool PaintCanvasSync::IsValidTransform(
    const FTransform& Transform,
    const double MaxTranslationDistance,
    const double MaxScale)
{
    if (Transform.ContainsNaN())
    {
        return false;
    }

    const FVector Translation = Transform.GetTranslation();
    if (Translation.SizeSquared()
        > FMath::Square(FMath::Max(MaxTranslationDistance, 0.0)))
    {
        return false;
    }

    const FVector Scale = Transform.GetScale3D().GetAbs();
    const double SafeMaxScale = FMath::Max(MaxScale, 0.01);
    return Scale.X >= 0.01
        && Scale.Y >= 0.01
        && Scale.Z >= 0.01
        && Scale.X <= SafeMaxScale
        && Scale.Y <= SafeMaxScale
        && Scale.Z <= SafeMaxScale;
}

bool PaintCanvasSync::IsValidFaceDecalPayload(
    const UMaterialInterface* FaceDecalMaterial,
    const FTransform& FaceDecalTransformOffset,
    const FVector& FaceDecalSize,
    const double MaxFaceDecalSize,
    const double MaxTranslationDistance,
    const double MaxScale)
{
    if (!FaceDecalMaterial || FaceDecalSize.ContainsNaN())
    {
        return false;
    }

    const double SafeMaxFaceDecalSize = FMath::Max(MaxFaceDecalSize, 1.0);
    if (FaceDecalSize.X <= 0.0
        || FaceDecalSize.Y <= 0.0
        || FaceDecalSize.Z <= 0.0
        || FaceDecalSize.X > SafeMaxFaceDecalSize
        || FaceDecalSize.Y > SafeMaxFaceDecalSize
        || FaceDecalSize.Z > SafeMaxFaceDecalSize)
    {
        return false;
    }

    return IsValidTransform(
        FaceDecalTransformOffset,
        MaxTranslationDistance,
        MaxScale);
}

uint32 PaintCanvasSync::AccumulateStrokeChecksum(
    uint32 CurrentChecksum,
    const FReplicatedPaintCanvasStroke& Stroke)
{
    CurrentChecksum = HashCombineFast(
        CurrentChecksum,
        GetTypeHash(Stroke.Sequence));
    CurrentChecksum = HashCombineFast(
        CurrentChecksum,
        GetTypeHash(Stroke.BrushSize));
    CurrentChecksum = HashCombineFast(
        CurrentChecksum,
        GetTypeHash(Stroke.DrawLocation.X));
    return HashCombineFast(
        CurrentChecksum,
        GetTypeHash(Stroke.DrawLocation.Y));
}

bool PaintCanvasSync::ValidateAuthoritativeHistory(
    TArray<const FReplicatedPaintCanvasStroke*>& InOutSortedStrokes,
    const FReplicatedPaintCanvasStateHeader& Header,
    const int32 MaxHistoryCount,
    uint32& OutChecksum)
{
    OutChecksum = 0;

    if (Header.Revision == 0
        || Header.LastSequence
            > static_cast<uint32>(FMath::Max(MaxHistoryCount, 1))
        || InOutSortedStrokes.Num()
            != static_cast<int32>(Header.LastSequence))
    {
        InOutSortedStrokes.Reset();
        return false;
    }

    InOutSortedStrokes.Sort(
        [](const FReplicatedPaintCanvasStroke& Left,
           const FReplicatedPaintCanvasStroke& Right)
        {
            return Left.Sequence < Right.Sequence;
        });

    uint32 ExpectedSequence = 1;
    for (const FReplicatedPaintCanvasStroke* Stroke : InOutSortedStrokes)
    {
        if (!Stroke
            || Stroke->Sequence != ExpectedSequence
            || Stroke->BrushSize <= 0.0f
            || !IsValidDrawLocation(FVector2D(Stroke->DrawLocation)))
        {
            InOutSortedStrokes.Reset();
            OutChecksum = 0;
            return false;
        }

        OutChecksum = AccumulateStrokeChecksum(OutChecksum, *Stroke);
        ++ExpectedSequence;
    }

    if (OutChecksum != Header.Checksum)
    {
        InOutSortedStrokes.Reset();
        OutChecksum = 0;
        return false;
    }

    return true;
}
