#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "UObject/Object.h"
#include "InfoPaintPreviewRenderer.generated.h"

class APdPlayer;
class AActor;
class UPaintCanvasWidget;
class UMaterialInstanceDynamic;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UImage;
class UPoseableMeshComponent;
class USceneComponent;
class UWorld;
struct FSkinWidgetSettings;

/** 로컬 표시 전용 복사본이다. 외형 적용·데이터 저장·게임플레이 RPC 전송은 수행하지 않는다. */
UCLASS()
class LABPROJECT_API UInfoPaintPreviewRenderer : public UObject
{
    GENERATED_BODY()

public:
    // Public API ------------------------------------------------------------------------------------------------------
    bool Show(APdPlayer* Player, UPaintCanvasWidget* Widget, UTextureRenderTarget2D* Canvas,
        const FSkinWidgetSettings& Settings);
    void Refresh();
    void Shutdown();

private:
    /** 월드 아래쪽에 충돌·복제 없는 미리보기 액터를 만든다. 실패하면 nullptr. */
    USceneComponent* SpawnPreviewActor(UWorld& World);
    /** 플레이어 메시를 기본 자세 그대로 복사한다. 실제 Pawn이 래그돌이어도 알아볼 수 있다. */
    UPoseableMeshComponent* AddPoseableCopy(USceneComponent& Root, const APdPlayer& Player);
    /** 칠한 그림의 불투명 복사본을 얼굴 데칼로 붙인다. 복사본 렌더 타깃을 만들지 못하면 false. */
    bool AddFaceDecal(USceneComponent& Root, const FVector& BaseLocation, APdPlayer& Player, UTextureRenderTarget2D& Canvas,
        const FSkinWidgetSettings& Settings);
    /** 얼굴을 비추는 조명과, 미리보기 액터만 찍는 캡처를 둔다. */
    void AddLightAndCapture(USceneComponent& Root, const FVector& Head);
    void ShowCaptureOnWidget(UPaintCanvasWidget& Widget, const FSkinWidgetSettings& Settings);

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
