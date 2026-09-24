#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;

// Validation shared by local drawing and completed drawing RPCs.
namespace PaintCanvasSync
{
    LABPROJECT_API double ClampBrushSize(double BrushSize, double MaxBrushSize);
    LABPROJECT_API bool IsValidDrawLocation(const FVector2D& DrawLocation);
    LABPROJECT_API bool IsValidTransform(const FTransform& Transform, double MaxTranslationDistance, double MaxScale);
    LABPROJECT_API bool IsValidFaceDecalPayload(
        const UMaterialInterface* FaceDecalMaterial,
        const FTransform& FaceDecalTransformOffset,
        const FVector& FaceDecalSize,
        double MaxFaceDecalSize,
        double MaxTranslationDistance,
        double MaxScale);
}
