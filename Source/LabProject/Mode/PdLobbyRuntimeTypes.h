#pragma once

#include "CoreMinimal.h"
#include "PdLobbyRuntimeTypes.generated.h"

class UMaterialInterface;
class UTexture2D;

USTRUCT(BlueprintType)
struct LABPROJECT_API FLobbyPaintCanvasStrokeCache
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UTexture2D> BrushTexture = nullptr;

	UPROPERTY()
	double BrushSize = 0.0;

	UPROPERTY()
	FVector2D DrawLocation = FVector2D::ZeroVector;

	UPROPERTY()
	bool bStartsNewStroke = true;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FLobbyPaintCanvasFaceDecalCache
{
	GENERATED_BODY()

	UPROPERTY()
	bool bHasFaceDecal = false;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> FaceDecalMaterial = nullptr;

	UPROPERTY()
	FName AttachSocketName = NAME_None;

	UPROPERTY()
	FTransform FaceDecalTransformOffset = FTransform::Identity;

	UPROPERTY()
	FVector FaceDecalSize = FVector::ZeroVector;

	UPROPERTY()
	FName TextureParameterName = NAME_None;

	UPROPERTY()
	int32 FaceDecalStrokeCount = 0;

	UPROPERTY()
	TArray<FLobbyPaintCanvasStrokeCache> Strokes;
};
