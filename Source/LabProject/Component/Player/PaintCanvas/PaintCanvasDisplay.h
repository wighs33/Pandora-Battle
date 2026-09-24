#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PaintCanvasDisplay.generated.h"

class APdPlayer;
class UDecalComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPrimitiveComponent;
class UTextureRenderTarget2D;

UCLASS(Transient)
class LABPROJECT_API UPaintCanvasDisplay : public UObject
{
    GENERATED_BODY()

public:
    // Public API ------------------------------------------------------------------------------------------------------
    void Initialize(APdPlayer* InPlayerOwner);
    void SetSpeechBubbleComponent(UPrimitiveComponent* InSpeechBubbleComponent);

    bool ShowSpeechBubble(UTextureRenderTarget2D* RenderTarget, int32 MaterialIndex, FName TextureParameterName);
    void HideSpeechBubble();

    bool ApplyFaceDecal(UTextureRenderTarget2D* PaintSnapshot, UMaterialInterface* FaceDecalMaterial, FName AttachSocketName, const FTransform& FaceDecalTransformOffset, FVector FaceDecalSize, FName TextureParameterName);
    void ClearFaceDecal();

    void Reset();

private:
    UPROPERTY(Transient)
    TWeakObjectPtr<APdPlayer> PlayerOwner;

    UPROPERTY(Transient)
    TObjectPtr<UPrimitiveComponent> SpeechBubbleComponent;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> ActiveSpeechBubbleMaterial;

    UPROPERTY(Transient)
    TObjectPtr<UTextureRenderTarget2D> ActiveSpeechBubbleRenderTarget;

    UPROPERTY(Transient)
    TObjectPtr<UDecalComponent> ActiveFaceDecalComponent;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> ActiveFaceDecalMaterial;

    UPROPERTY(Transient)
    TObjectPtr<UTextureRenderTarget2D> ActiveFaceDecalSnapshot;
};
