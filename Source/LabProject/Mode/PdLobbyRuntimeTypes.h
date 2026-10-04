#pragma once

#include "CoreMinimal.h"
#include "Common/Enum_Direction.h"
#include "GameplayTagContainer.h"
#include "PdLobbyRuntimeTypes.generated.h"

class UMaterialInterface;
class UTexture2D;

/** 로비에서 고른 장착 스킨과 좌·상·우 판도라 슬롯. 경기 서버가 같은 외형과 슬롯을 복구하는 데 쓴다. */
USTRUCT()
struct LABPROJECT_API FLobbyTravelHandoff
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<FGameplayTag, FName> EquippedSkinNamesBySlot;

	UPROPERTY()
	TMap<EEnum_Direction, FName> PandoraNamesByDirection;
};

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
