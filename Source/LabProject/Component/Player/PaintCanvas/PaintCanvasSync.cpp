#include "Component/Player/PaintCanvas/PaintCanvasSync.h"

#include "Materials/MaterialInterface.h"

double PaintCanvasSync::ClampBrushSize(const double BrushSize, const double MaxBrushSize)
{
    if (!FMath::IsFinite(BrushSize) || BrushSize <= 0.0)
    {
        return 0.0;
    }

    return FMath::Clamp(BrushSize, 1.0, FMath::Max(MaxBrushSize, 1.0));
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
    if (Translation.SizeSquared() > FMath::Square(FMath::Max(MaxTranslationDistance, 0.0)))
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

    return IsValidTransform(FaceDecalTransformOffset, MaxTranslationDistance, MaxScale);
}
