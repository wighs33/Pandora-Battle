#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "UObject/Object.h"
#include "InfoPaintPreview.generated.h"

class APdPlayer;
class AActor;
class UPaintCanvasWidget;
class UMaterialInstanceDynamic;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UImage;
struct FSkinWidgetSettings;

/** Local display-only copy. Never applies cosmetics, saves data or sends gameplay RPCs. */
UCLASS()
class LABPROJECT_API UInfoPaintPreview : public UObject
{
    GENERATED_BODY()

public:
    // Public API ------------------------------------------------------------------------------------------------------
    bool Show(APdPlayer* Player, UPaintCanvasWidget* Widget, UTextureRenderTarget2D* Canvas,
        const FSkinWidgetSettings& Settings);
    void Refresh();
    void Shutdown();

private:
    UPROPERTY(Transient) TObjectPtr<AActor> PreviewActor;
    UPROPERTY(Transient) TObjectPtr<USceneCaptureComponent2D> Capture;
    UPROPERTY(Transient) TObjectPtr<UTextureRenderTarget2D> Target;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> DisplayMaterial;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> DecalMaterial;
    UPROPERTY(Transient) TObjectPtr<UTextureRenderTarget2D> SourceCanvas;
    UPROPERTY(Transient) TObjectPtr<UTextureRenderTarget2D> PreviewCanvas;
    TWeakObjectPtr<UImage> PreviewImage;
    FTSTicker::FDelegateHandle InitialCaptureHandle;
};
