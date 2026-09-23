#include "UI/Controller/InfoPaintPreview.h"
#include "Character/PdPlayer.h"
#include "Components/DecalComponent.h"
#include "Components/Image.h"
#include "Components/PointLightComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Definition/UI/WidgetClassDefinition.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UI/Widget/PaintCanvasWidget.h"

bool UInfoPaintPreview::Show(APdPlayer* Player, UPaintCanvasWidget* Widget,
    UTextureRenderTarget2D* Canvas, const FSkinWidgetSettings& Settings)
{
    Shutdown();
    if (!Player || !Widget || !Widget->GetFacePreviewImage() || !Canvas
        || !Player->GetMesh() || !Settings.PaintCanvasDisplayMaterial
        || !Settings.PaintCanvasFaceDecalMaterial) return false;

    FActorSpawnParameters Spawn;
    Spawn.ObjectFlags |= RF_Transient;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    PreviewActor = Player->GetWorld()->SpawnActor<AActor>(AActor::StaticClass(),
        FVector(0, 0, -100000), FRotator::ZeroRotator, Spawn);
    if (!PreviewActor) return false;
    PreviewActor->Tags.Add(TEXT("InfoPaintPreview"));
    PreviewActor->SetReplicates(false);
    PreviewActor->SetActorEnableCollision(false);
    USceneComponent* Root = NewObject<USceneComponent>(PreviewActor);
    PreviewActor->SetRootComponent(Root);
    Root->RegisterComponent();
    Root->SetWorldLocation(FVector(0, 0, -100000));

    USkeletalMeshComponent* Source = Player->GetMesh();
    UPoseableMeshComponent* Mesh = NewObject<UPoseableMeshComponent>(PreviewActor);
    Mesh->SetupAttachment(Root);
    Mesh->SetSkinnedAssetAndUpdate(Source->GetSkeletalMeshAsset());
    // A fixed reference pose remains readable even when the actual pawn is ragdolled.
    const APdPlayer* DefaultPlayer = Player->GetClass()->GetDefaultObject<APdPlayer>();
    Mesh->SetRelativeTransform(DefaultPlayer->GetMesh()->GetRelativeTransform());
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetCastShadow(false);
    Mesh->RegisterComponent();
    for (int32 Index = 0; Index < Source->GetNumMaterials(); ++Index)
        Mesh->SetMaterial(Index, Source->GetMaterial(Index));

    FName Socket = Settings.PaintCanvasFaceDecalSocketName;
    if (!Mesh->DoesSocketExist(Socket)) Socket = NAME_None;
    const FVector Head = Socket.IsNone() ? Mesh->Bounds.Origin + FVector(0,0,35)
        : Mesh->GetSocketLocation(Socket);
    DecalMaterial = UMaterialInstanceDynamic::Create(Settings.PaintCanvasFaceDecalMaterial.Get(), this);
    const FName Parameter = Settings.PaintCanvasFaceDecalTextureParameterName.IsNone()
        ? FName(TEXT("RenderTarget")) : Settings.PaintCanvasFaceDecalTextureParameterName;
    SourceCanvas = Canvas;
    PreviewCanvas = UKismetRenderingLibrary::CreateRenderTarget2D(Player,
        Canvas->SizeX, Canvas->SizeY, RTF_RGBA16f, FLinearColor::Transparent);
    if (!PreviewCanvas) { Shutdown(); return false; }
    DecalMaterial->SetTextureParameterValue(Parameter, PreviewCanvas);
    UDecalComponent* Decal = NewObject<UDecalComponent>(PreviewActor);
    Decal->SetupAttachment(Root);
    Decal->SetDecalMaterial(DecalMaterial);
    Decal->DecalSize = Settings.PaintCanvasFaceDecalSize.ComponentMax(FVector::OneVector);
    Decal->SetFadeScreenSize(0);
    Decal->RegisterComponent();
    // Match the actual application's actor-oriented socket transform, including projection depth.
    const FVector BaseLocation = Socket.IsNone() ? Root->GetComponentLocation() : Head;
    FTransform DecalTransform = Settings.PaintCanvasFaceDecalTransformOffset
        * FTransform(FQuat::Identity, BaseLocation, FVector::OneVector);
    DecalTransform.AddToTranslation(DecalTransform.GetUnitAxis(EAxis::X) * Decal->DecalSize.X * .5);
    Decal->SetWorldTransform(DecalTransform);

    UPointLightComponent* Light = NewObject<UPointLightComponent>(PreviewActor);
    Light->SetupAttachment(Root);
    Light->SetIntensity(20000);
    Light->SetAttenuationRadius(700);
    Light->SetCastShadows(false);
    Light->RegisterComponent();
    Light->SetWorldLocation(Head + FVector(120, -65, 90));

    Target = NewObject<UTextureRenderTarget2D>(this);
    Target->ClearColor = FLinearColor(.025f, .016f, .035f, 1.f);
    Target->InitAutoFormat(512, 512);
    Capture = NewObject<USceneCaptureComponent2D>(PreviewActor);
    Capture->SetupAttachment(Root);
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
    Capture->SetWorldLocationAndRotation(CameraPosition,
        (Head + FVector(0,0,-12) - CameraPosition).Rotation());
    DisplayMaterial = UMaterialInstanceDynamic::Create(Settings.PaintCanvasDisplayMaterial.Get(), this);
    DisplayMaterial->SetTextureParameterValue(TEXT("CanvasTexture"), Target);
    PreviewImage = Widget->GetFacePreviewImage();
    FSlateBrush PreviewBrush;
    PreviewBrush.SetResourceObject(DisplayMaterial);
    PreviewBrush.DrawAs = ESlateBrushDrawType::Image;
    PreviewBrush.TintColor = FSlateColor(FLinearColor::White);
    PreviewBrush.ImageSize = FVector2D(512,512);
    PreviewImage->SetBrush(PreviewBrush);
    Refresh();
    // Give newly registered scene components and their render resources a frame to initialize.
    // The core ticker also runs while the Info screen has paused gameplay.
    InitialCaptureHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateWeakLambda(this, [this](float)
        {
            Refresh();
            InitialCaptureHandle.Reset();
            return false;
        }), .15f);
    return true;
}

void UInfoPaintPreview::Refresh()
{
    // Use the same opaque copy as the real face application; the shared paint target's
    // accumulated alpha is intentionally not the face decal's opacity.
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

void UInfoPaintPreview::Shutdown()
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
