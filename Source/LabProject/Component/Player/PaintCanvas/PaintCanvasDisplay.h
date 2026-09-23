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

/**
 * 완성된 Paint RenderTarget을 월드 표현(Speech Bubble / Face Decal)에 연결한다.
 * Stroke 생성, 네트워크 동기화, RenderTarget 그리기는 담당하지 않는다.
 */
UCLASS(Transient)
class LABPROJECT_API UPaintCanvasDisplay : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(APdPlayer* InPlayerOwner);
    void SetSpeechBubbleComponent(UPrimitiveComponent* InSpeechBubbleComponent);

    bool ShowSpeechBubble(
        UTextureRenderTarget2D* RenderTarget,
        FName SpeechBubbleComponentName,
        int32 MaterialIndex,
        FName TextureParameterName);
    void HideSpeechBubble(FName SpeechBubbleComponentName);

    bool ApplyFaceDecal(
        UTextureRenderTarget2D* PaintSnapshot,
        UMaterialInterface* FaceDecalMaterial,
        FName AttachSocketName,
        const FTransform& FaceDecalTransformOffset,
        FVector FaceDecalSize,
        FName TextureParameterName);
    void ClearFaceDecal();

    void Reset(FName SpeechBubbleComponentName);

private:
    UPrimitiveComponent* FindSpeechBubbleComponent(FName ComponentName) const;

    UPROPERTY(Transient)
    TWeakObjectPtr<APdPlayer> PlayerOwner;

    UPROPERTY(Transient)
    TObjectPtr<UPrimitiveComponent> SpeechBubbleComponent;

    UPROPERTY(Transient)
    TObjectPtr<UPrimitiveComponent> ActiveSpeechBubbleComponent;

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
