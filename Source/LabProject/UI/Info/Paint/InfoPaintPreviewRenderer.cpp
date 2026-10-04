#include "UI/Info/Paint/InfoPaintPreviewRenderer.h"
#include "Character/PdPlayer.h"
#include "Components/DecalComponent.h"
#include "Components/Image.h"
#include "Components/PointLightComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "UI/Core/WidgetClassDefinition.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UI/Info/Paint/PaintCanvasWidget.h"

bool UInfoPaintPreviewRenderer::Show(APdPlayer* Player, UPaintCanvasWidget* Widget,
    UTextureRenderTarget2D* Canvas, const FSkinWidgetSettings& Settings)
{
    Shutdown();
    if (!Player || !Widget || !Widget->GetFacePreviewImage() || !Canvas
        || !Player->GetMesh() || !Settings.PaintCanvasDisplayMaterial
        || !Settings.PaintCanvasFaceDecalMaterial) return false;

    USceneComponent* Root = SpawnPreviewActor(*Player->GetWorld());
    if (!Root) return false;
    const UPoseableMeshComponent* Mesh = AddPoseableCopy(*Root, *Player);

    FName Socket = Settings.PaintCanvasFaceDecalSocketName;
    if (!Mesh->DoesSocketExist(Socket)) Socket = NAME_None;
    const FVector Head = Socket.IsNone() ? Mesh->Bounds.Origin + FVector(0,0,35)
        : Mesh->GetSocketLocation(Socket);
    // 투영 깊이까지 포함해, 실제 적용과 같은 액터 기준 소켓 변환을 맞춘다.
    const FVector DecalBaseLocation = Socket.IsNone() ? Root->GetComponentLocation() : Head;
    if (!AddFaceDecal(*Root, DecalBaseLocation, *Player, *Canvas, Settings)) { Shutdown(); return false; }

    AddLightAndCapture(*Root, Head);
    ShowCaptureOnWidget(*Widget, Settings);
    Refresh();
    // 새로 등록한 씬 컴포넌트와 렌더 리소스가 초기화되도록 한 프레임을 준다.
    // 코어 ticker는 정보창이 게임플레이를 멈춘 동안에도 돈다.
    InitialCaptureHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float)
        {
            Refresh();
            InitialCaptureHandle.Reset();
            return false;
        }), .15f);
    return true;
}

USceneComponent* UInfoPaintPreviewRenderer::SpawnPreviewActor(UWorld& World)
{
    FActorSpawnParameters Spawn;
    Spawn.ObjectFlags |= RF_Transient;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    PreviewActor = World.SpawnActor<AActor>(AActor::StaticClass(), FVector(0, 0, -100000), FRotator::ZeroRotator, Spawn);
    if (!PreviewActor) return nullptr;
    PreviewActor->Tags.Add(TEXT("InfoPaintPreview"));
    PreviewActor->SetReplicates(false);
    PreviewActor->SetActorEnableCollision(false);
    USceneComponent* Root = NewObject<USceneComponent>(PreviewActor);
    PreviewActor->SetRootComponent(Root);
    Root->RegisterComponent();
    Root->SetWorldLocation(FVector(0, 0, -100000));
    return Root;
}

UPoseableMeshComponent* UInfoPaintPreviewRenderer::AddPoseableCopy(USceneComponent& Root, const APdPlayer& Player)
{
    USkeletalMeshComponent* Source = Player.GetMesh();
    UPoseableMeshComponent* Mesh = NewObject<UPoseableMeshComponent>(PreviewActor);
    Mesh->SetupAttachment(&Root);
    Mesh->SetSkinnedAssetAndUpdate(Source->GetSkeletalMeshAsset());
    // 실제 Pawn이 래그돌 상태여도 알아볼 수 있도록 고정된 기준 자세를 쓴다.
    const APdPlayer* DefaultPlayer = Player.GetClass()->GetDefaultObject<APdPlayer>();
    Mesh->SetRelativeTransform(DefaultPlayer->GetMesh()->GetRelativeTransform());
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetCastShadow(false);
    Mesh->RegisterComponent();
    for (int32 Index = 0; Index < Source->GetNumMaterials(); ++Index)
        Mesh->SetMaterial(Index, Source->GetMaterial(Index));
    return Mesh;
}

bool UInfoPaintPreviewRenderer::AddFaceDecal(USceneComponent& Root, const FVector& BaseLocation, APdPlayer& Player,
    UTextureRenderTarget2D& Canvas, const FSkinWidgetSettings& Settings)
{
    DecalMaterial = UMaterialInstanceDynamic::Create(Settings.PaintCanvasFaceDecalMaterial.Get(), this);
    const FName Parameter = Settings.PaintCanvasFaceDecalTextureParameterName.IsNone()
        ? FName(TEXT("RenderTarget")) : Settings.PaintCanvasFaceDecalTextureParameterName;
    SourceCanvas = &Canvas;
    PreviewCanvas = UKismetRenderingLibrary::CreateRenderTarget2D(&Player,
        Canvas.SizeX, Canvas.SizeY, RTF_RGBA16f, FLinearColor::Transparent);
    if (!PreviewCanvas) return false;
    DecalMaterial->SetTextureParameterValue(Parameter, PreviewCanvas);
    UDecalComponent* Decal = NewObject<UDecalComponent>(PreviewActor);
    Decal->SetupAttachment(&Root);
    Decal->SetDecalMaterial(DecalMaterial);
    Decal->DecalSize = Settings.PaintCanvasFaceDecalSize.ComponentMax(FVector::OneVector);
    Decal->SetFadeScreenSize(0);
    Decal->RegisterComponent();
    FTransform DecalTransform = Settings.PaintCanvasFaceDecalTransformOffset
        * FTransform(FQuat::Identity, BaseLocation, FVector::OneVector);
    DecalTransform.AddToTranslation(DecalTransform.GetUnitAxis(EAxis::X) * Decal->DecalSize.X * .5);
    Decal->SetWorldTransform(DecalTransform);
    return true;
}

void UInfoPaintPreviewRenderer::AddLightAndCapture(USceneComponent& Root, const FVector& Head)
{
    UPointLightComponent* Light = NewObject<UPointLightComponent>(PreviewActor);
    Light->SetupAttachment(&Root);
    Light->SetIntensity(20000);
    Light->SetAttenuationRadius(700);
    Light->SetCastShadows(false);
    Light->RegisterComponent();
    Light->SetWorldLocation(Head + FVector(120, -65, 90));

    Target = NewObject<UTextureRenderTarget2D>(this);
    Target->ClearColor = FLinearColor(.025f, .016f, .035f, 1.f);
    Target->InitAutoFormat(512, 512);
    Capture = NewObject<USceneCaptureComponent2D>(PreviewActor);
    Capture->SetupAttachment(&Root);
    Capture->TextureTarget = Target;
    Capture->bCaptureEveryFrame = false;
    Capture->bCaptureOnMovement = false;
    Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
    Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
    Capture->FOVAngle = 35;
    Capture->PostProcessSettings.bOverride_AutoExposureMethod = true;
    Capture->PostProcessSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
    Capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
    Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
    Capture->PostProcessSettings.bOverride_AutoExposureBias = true;
    Capture->PostProcessSettings.AutoExposureBias = 1;
    Capture->ShowFlags.SetAtmosphere(false);
    Capture->ShowFlags.SetFog(false);
    Capture->ShowFlags.SetMotionBlur(false);
    Capture->RegisterComponent();
    Capture->ShowOnlyActorComponents(PreviewActor);
    const FVector CameraPosition = Head + FVector(175, 0, 8);
    Capture->SetWorldLocationAndRotation(CameraPosition, (Head + FVector(0,0,-12) - CameraPosition).Rotation());
}

void UInfoPaintPreviewRenderer::ShowCaptureOnWidget(UPaintCanvasWidget& Widget, const FSkinWidgetSettings& Settings)
{
    DisplayMaterial = UMaterialInstanceDynamic::Create(Settings.PaintCanvasDisplayMaterial.Get(), this);
    DisplayMaterial->SetTextureParameterValue(TEXT("CanvasTexture"), Target);
    PreviewImage = Widget.GetFacePreviewImage();
    FSlateBrush PreviewBrush;
    PreviewBrush.SetResourceObject(DisplayMaterial);
    PreviewBrush.DrawAs = ESlateBrushDrawType::Image;
    PreviewBrush.TintColor = FSlateColor(FLinearColor::White);
    PreviewBrush.ImageSize = FVector2D(512,512);
    PreviewImage->SetBrush(PreviewBrush);
}

void UInfoPaintPreviewRenderer::Refresh()
{
    // 실제 얼굴 적용과 같은 불투명 복사본을 쓴다. 공용 칠하기 타깃에 쌓인 알파는
    // 얼굴 데칼의 불투명도가 아니다.
    if (IsValid(PreviewActor) && SourceCanvas && PreviewCanvas)
    {
        UCanvas* DrawCanvas = nullptr;
        FVector2D Size;
        FDrawToRenderTargetContext Context;
        UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(PreviewActor, PreviewCanvas, DrawCanvas, Size, Context);
        if (DrawCanvas) DrawCanvas->K2_DrawTexture(SourceCanvas, FVector2D::ZeroVector, Size,
            FVector2D::ZeroVector, FVector2D(1,1), FLinearColor::White, BLEND_Opaque);
        UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(PreviewActor, Context);
    }
    if (IsValid(Capture)) Capture->CaptureScene();
}

void UInfoPaintPreviewRenderer::Shutdown()
{
    FTSTicker::RemoveTicker(InitialCaptureHandle);
    InitialCaptureHandle.Reset();
    if (PreviewImage.IsValid()) PreviewImage->SetBrush(FSlateBrush());
    PreviewImage.Reset();
    if (IsValid(PreviewActor)) PreviewActor->Destroy();
    PreviewActor = nullptr;
    Capture = nullptr;
    Target = nullptr;
    DisplayMaterial = nullptr;
    DecalMaterial = nullptr;
    SourceCanvas = nullptr;
    PreviewCanvas = nullptr;
}
