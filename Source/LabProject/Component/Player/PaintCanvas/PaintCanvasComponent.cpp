#include "Component/Player/PaintCanvas/PaintCanvasComponent.h"

#include "Character/PdPlayer.h"
#include "Component/Player/PaintCanvas/PaintCanvasDisplay.h"
#include "Component/Player/PaintCanvas/PaintCanvasSync.h"
#include "Definition/UI/WidgetClassDefinition.h"
#include "Engine/Canvas.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Lobby/LobbyRuntimeSubsystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PaintCanvasComponent)

DEFINE_LOG_CATEGORY_STATIC(PdPaintCanvasComponentLog, Log, All);

namespace
{
    const FLinearColor OpaqueBlackBrushColor(0.0f, 0.0f, 0.0f, 1.0f);
    constexpr double PaintBrushSpacingRatio = 0.25;
}

UPaintCanvasComponent::UPaintCanvasComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UPaintCanvasComponent::BeginPlay()
{
    Super::BeginPlay();
    GetOrCreatePresentation();
    HidePaintSpeechBubble();
}

void UPaintCanvasComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    CancelPaintCanvasExport();
    if (Presentation)
    {
        Presentation->Reset(PaintSpeechBubbleComponentName);
    }

    Super::EndPlay(EndPlayReason);
}

void UPaintCanvasComponent::SetSpeechBubbleComponent(UPrimitiveComponent* InSpeechBubbleComponent)
{
    if (UPaintCanvasDisplay* PaintPresentation = GetOrCreatePresentation())
    {
        PaintPresentation->SetSpeechBubbleComponent(InSpeechBubbleComponent);
    }
}

UPaintCanvasDisplay* UPaintCanvasComponent::GetOrCreatePresentation()
{
    if (!Presentation)
    {
        Presentation = NewObject<UPaintCanvasDisplay>(this);
    }

    if (Presentation)
    {
        Presentation->Initialize(GetPlayerOwner());
    }

    return Presentation.Get();
}

bool UPaintCanvasComponent::EnsurePaintCanvasRenderResources(const bool bResetCanvas)
{
    if (!PaintCanvasRenderTarget)
    {
       PaintCanvasRenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(
          this,
          FMath::Max(RenderTargetWidth, 1),
          FMath::Max(RenderTargetHeight, 1),
          RTF_RGBA16f,
          ClearColor,
          false,
          false);
    }

    if (!PaintBrushMaterial)
    {
       const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this);
       UMaterialInterface* BrushMaterialParent = WidgetDefinition
           ? WidgetDefinition->GetSkinWidgetSettings().PaintBrushMaterial.LoadSynchronous()
           : nullptr;
       if (BrushMaterialParent)
       {
          PaintBrushMaterial = UMaterialInstanceDynamic::Create(BrushMaterialParent, this);
       }
    }

    ApplyBrushMaterialParameters();
    if (bResetCanvas)
    {
       ResetPaintCanvasRenderTarget();
    }

    return PaintCanvasRenderTarget && PaintBrushMaterial;
}

void UPaintCanvasComponent::ResetPaintCanvasRenderTarget()
{
    ResetPaintStroke();
    if (PaintCanvasRenderTarget)
    {
       UKismetRenderingLibrary::ClearRenderTarget2D(this, PaintCanvasRenderTarget, ClearColor);
    }
}

bool UPaintCanvasComponent::DrawBrushToRenderTarget(
    UTexture2D* InBrushTexture,
    const double InBrushSize,
    const FVector2D& DrawLocation)
{
    const double SafeBrushSize = PaintCanvasSync::ClampBrushSize(InBrushSize, MaxReplicatedPaintBrushSize);
    if (!PaintCanvasRenderTarget
        || !PaintBrushMaterial
        || SafeBrushSize <= 0.0
        || !PaintCanvasSync::IsValidDrawLocation(DrawLocation))
    {
        return false;
    }

    if (InBrushTexture)
    {
        PaintBrushMaterial->SetTextureParameterValue(BrushTextureParameterName, InBrushTexture);
    }
    ApplyBrushMaterialParameters();

    UCanvas* DrawCanvas = nullptr;
    FVector2D RenderTargetSize = FVector2D::ZeroVector;
    FDrawToRenderTargetContext Context;
    UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(
        this,
        PaintCanvasRenderTarget,
        DrawCanvas,
        RenderTargetSize,
        Context);

    if (DrawCanvas)
    {
        const FVector2D BrushScreenSize(SafeBrushSize, SafeBrushSize);
        const FVector2D BrushScreenPosition = RenderTargetSize * DrawLocation - BrushScreenSize * 0.5;
        DrawCanvas->K2_DrawMaterial(
            PaintBrushMaterial,
            BrushScreenPosition,
            BrushScreenSize,
            FVector2D::ZeroVector,
            FVector2D(1.0, 1.0),
            0.0f,
            FVector2D(0.5, 0.5));
    }

    UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Context);
    return DrawCanvas != nullptr;
}

bool UPaintCanvasComponent::DrawPaintStroke(const FPaintCanvasStroke& Stroke, const FPaintCanvasStroke* PreviousStroke)
{
    if (!PaintCanvasRenderTarget)
    {
        return false;
    }

    const FVector2D EndLocation(Stroke.DrawLocation);
    const FVector2D StartLocation = PreviousStroke && !Stroke.bStartsNewStroke
        ? FVector2D(PreviousStroke->DrawLocation) : EndLocation;
    const FVector2D CanvasSize(PaintCanvasRenderTarget->SizeX, PaintCanvasRenderTarget->SizeY);
    const double DistancePixels = ((EndLocation - StartLocation) * CanvasSize).Size();
    const double SpacingPixels = FMath::Max(Stroke.BrushSize * PaintBrushSpacingRatio, 1.0);
    const int32 StampCount = FMath::Max(FMath::CeilToInt(DistancePixels / SpacingPixels), 1);

    for (int32 StampIndex = 1; StampIndex <= StampCount; ++StampIndex)
    {
        const double Alpha = static_cast<double>(StampIndex) / StampCount;
        const FVector2D StampLocation = FMath::Lerp(StartLocation, EndLocation, Alpha);
        if (!DrawBrushToRenderTarget(BrushTexture.Get(), Stroke.BrushSize, StampLocation))
        {
            return false;
        }
    }
    return true;
}

void UPaintCanvasComponent::ApplyBrushMaterialParameters() const
{
    if (!PaintBrushMaterial)
    {
       return;
    }

    PaintBrushMaterial->SetVectorParameterValue(BrushColorParameterName, OpaqueBlackBrushColor);
    PaintBrushMaterial->SetVectorParameterValue(TEXT("BaseColor"), OpaqueBlackBrushColor);
}

bool UPaintCanvasComponent::BeginPaintCanvasUiSession()
{
    CancelPaintCanvasExport();
    bPaintCanvasUiSessionActive = false;

    APdPlayer* PlayerOwner = GetPlayerOwner();
    if (!PlayerOwner || !PlayerOwner->IsLocallyControlled() || !EnsurePaintCanvasRenderResources(true))
    {
        return false;
    }

    LocalPaintStrokes.Reset();
    bPaintCanvasUiSessionActive = true;
    return true;
}

bool UPaintCanvasComponent::PaintAtNormalizedLocation(const FVector2D& DrawLocation)
{
    if (!bPaintCanvasUiSessionActive
        || LocalPaintStrokes.Num() >= FMath::Clamp(MaxReplicatedPaintStrokeHistory, 1, 8192)
        || !PaintCanvasSync::IsValidDrawLocation(DrawLocation))
    {
        return false;
    }

    FPaintCanvasStroke Stroke;
    Stroke.BrushSize = static_cast<float>(PaintCanvasSync::ClampBrushSize(BrushSize, MaxReplicatedPaintBrushSize));
    Stroke.DrawLocation = FVector2f(DrawLocation);
    Stroke.bStartsNewStroke = !bHasPreviousPaintLocation;
    const FPaintCanvasStroke* PreviousStroke = LocalPaintStrokes.IsEmpty() ? nullptr : &LocalPaintStrokes.Last();
    if (!DrawPaintStroke(Stroke, PreviousStroke))
    {
        return false;
    }

    LocalPaintStrokes.Add(Stroke);
    bHasPreviousPaintLocation = true;
    return true;
}

void UPaintCanvasComponent::ResetPaintStroke()
{
    bHasPreviousPaintLocation = false;
}

void UPaintCanvasComponent::HidePaintCanvas()
{
    ResetPaintStroke();
    bPaintCanvasUiSessionActive = false;
}

bool UPaintCanvasComponent::HasActivePaintCanvas() const
{
    return bPaintCanvasUiSessionActive || bPaintCanvasExportActive;
}

bool UPaintCanvasComponent::ExportActivePaintCanvasToSpeechBubble()
{
    if (!StartLocalPaintCanvasExport())
    {
       return false;
    }

    bPaintCanvasUiSessionActive = false;
    SubmitPaintCanvasExportForNetwork();
    ResetPaintStroke();
    return true;
}

void UPaintCanvasComponent::CancelPaintCanvasExport()
{
    if (UWorld* World = GetWorld())
    {
       World->GetTimerManager().ClearTimer(PaintCanvasExportTimerHandle);
    }
    PaintCanvasExportTimerHandle.Invalidate();
    bPaintCanvasExportActive = false;
    HidePaintSpeechBubble();
}

void UPaintCanvasComponent::HandlePaintCanvasExportExpired()
{
    CancelPaintCanvasExport();
}

bool UPaintCanvasComponent::StartLocalPaintCanvasExport()
{
    if (!EnsurePaintCanvasRenderResources())
    {
       return false;
    }
    CancelPaintCanvasExport();

    if (!ApplyPaintCanvasToSpeechBubble())
    {
       return false;
    }

    bPaintCanvasExportActive = true;

    if (UWorld* World = GetWorld())
    {
       World->GetTimerManager().SetTimer(
          PaintCanvasExportTimerHandle,
          this,
          &ThisClass::HandlePaintCanvasExportExpired,
          static_cast<float>(FMath::Max(PaintCanvasExportDuration, 0.1)),
          false);
    }

    return true;
}

bool UPaintCanvasComponent::TryConsumePaintNetworkEvent(double& LastAcceptedTime, const double MinInterval)
{
    UWorld* World = GetWorld();
    if (!World)
    {
       return false;
    }

    const double CurrentTime = World->GetTimeSeconds();
    const double SafeMinInterval = FMath::Max(MinInterval, 0.0);
    if (CurrentTime - LastAcceptedTime < SafeMinInterval)
    {
       return false;
    }

    LastAcceptedTime = CurrentTime;
    return true;
}

UTextureRenderTarget2D* UPaintCanvasComponent::CreatePaintCanvasCopy(
    UTextureRenderTarget2D* SourceRenderTarget,
    const double Scale,
    const FLinearColor& ClearTargetColor)
{
    if (!SourceRenderTarget)
    {
       return nullptr;
    }

    UTextureRenderTarget2D* CopyRenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(
       this,
       FMath::Max(SourceRenderTarget->SizeX, 1),
       FMath::Max(SourceRenderTarget->SizeY, 1),
       RTF_RGBA16f,
       ClearTargetColor,
       false,
       false);
    if (!CopyRenderTarget)
    {
       return nullptr;
    }

    UCanvas* DrawCanvas = nullptr;
    FVector2D RenderTargetSize = FVector2D::ZeroVector;
    FDrawToRenderTargetContext Context;
    UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(
       this,
       CopyRenderTarget,
       DrawCanvas,
       RenderTargetSize,
       Context);

    if (DrawCanvas)
    {
       const double SafeScale = FMath::Clamp(Scale, 0.01, 1.0);
       const FVector2D DrawSize = RenderTargetSize * SafeScale;
       const FVector2D DrawPosition = (RenderTargetSize - DrawSize) * 0.5;

       DrawCanvas->K2_DrawTexture(
          SourceRenderTarget,
          DrawPosition,
          DrawSize,
          FVector2D::ZeroVector,
          FVector2D(1.0, 1.0),
          FLinearColor::White,
          BLEND_Opaque,
          0.0f,
          FVector2D(0.5, 0.5));
    }

    UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Context);
    return CopyRenderTarget;
}

bool UPaintCanvasComponent::ApplyPaintCanvasToSpeechBubble()
{
    UTextureRenderTarget2D* PaintRenderTarget = GetActivePaintCanvasRenderTarget();
    if (!PaintRenderTarget)
    {
        UE_LOG(
            PdPaintCanvasComponentLog,
            Warning,
            TEXT("Failed to export paint canvas to speech bubble. RenderTarget=None"));
        return false;
    }

    UTextureRenderTarget2D* SpeechBubbleRenderTarget =
        CreatePaintCanvasCopy(PaintRenderTarget, PaintSpeechBubbleRenderTargetScale, FLinearColor::White);
    if (!SpeechBubbleRenderTarget)
    {
        SpeechBubbleRenderTarget = PaintRenderTarget;
    }

    UPaintCanvasDisplay* PaintPresentation = GetOrCreatePresentation();
    return PaintPresentation
        && PaintPresentation->ShowSpeechBubble(
            SpeechBubbleRenderTarget,
            PaintSpeechBubbleComponentName,
            PaintSpeechBubbleMaterialIndex,
            PaintSpeechBubbleRenderTargetParameterName);
}

void UPaintCanvasComponent::HidePaintSpeechBubble()
{
    if (UPaintCanvasDisplay* PaintPresentation = GetOrCreatePresentation())
    {
        PaintPresentation->HideSpeechBubble(PaintSpeechBubbleComponentName);
    }
}

UTextureRenderTarget2D* UPaintCanvasComponent::GetActivePaintCanvasRenderTarget() const
{
    return PaintCanvasRenderTarget.Get();
}

bool UPaintCanvasComponent::IsValidPaintCanvasStrokes(const TArray<FPaintCanvasStroke>& Strokes) const
{
    if (Strokes.Num() > FMath::Clamp(MaxReplicatedPaintStrokeHistory, 1, 8192))
    {
        return false;
    }

    for (const FPaintCanvasStroke& Stroke : Strokes)
    {
        if (!FMath::IsFinite(Stroke.BrushSize)
            || Stroke.BrushSize <= 0.0f
            || Stroke.BrushSize > FMath::Max(MaxReplicatedPaintBrushSize, 1.0)
            || !PaintCanvasSync::IsValidDrawLocation(FVector2D(Stroke.DrawLocation)))
        {
            return false;
        }
    }
    return true;
}

bool UPaintCanvasComponent::ReplayPaintCanvasStrokes(const TArray<FPaintCanvasStroke>& Strokes)
{
    if (!EnsurePaintCanvasRenderResources(true))
    {
        return false;
    }

    const FPaintCanvasStroke* PreviousStroke = nullptr;
    for (const FPaintCanvasStroke& Stroke : Strokes)
    {
        if (!DrawPaintStroke(Stroke, PreviousStroke))
        {
            return false;
        }
        PreviousStroke = &Stroke;
    }
    return true;
}

void UPaintCanvasComponent::SubmitPaintCanvasExportForNetwork()
{
    const APdPlayer* PlayerOwner = GetPlayerOwner();
    if (PlayerOwner && PlayerOwner->GetNetMode() != NM_Standalone)
    {
        ServerExportPaintCanvas(LocalPaintStrokes);
    }
}

void UPaintCanvasComponent::CacheLocalPaintCanvasFaceDecalForTravel(
    UMaterialInterface* FaceDecalMaterial,
    const FName AttachSocketName,
    const FTransform& FaceDecalTransformOffset,
    FVector FaceDecalSize,
    const FName TextureParameterName) const
{
    const APdPlayer* PlayerOwner = GetPlayerOwner();
    if (!PlayerOwner || !PlayerOwner->IsLocallyControlled())
    {
       return;
    }

    if (ULobbyRuntimeSubsystem* LobbySubsystem = UGameInstance::GetSubsystem<ULobbyRuntimeSubsystem>(
       PlayerOwner->GetGameInstance()))
    {
       LobbySubsystem->ResetLocalLobbyPaintCanvasCache();
       for (const FPaintCanvasStroke& Stroke : LocalPaintStrokes)
       {
           LobbySubsystem->CacheLocalLobbyPaintCanvasStroke(
               BrushTexture.Get(), Stroke.BrushSize, FVector2D(Stroke.DrawLocation), Stroke.bStartsNewStroke);
       }
       LobbySubsystem->CacheLocalLobbyPaintCanvasFaceDecal(
          FaceDecalMaterial,
          AttachSocketName,
          FaceDecalTransformOffset,
          FaceDecalSize,
          TextureParameterName);
    }
}

bool UPaintCanvasComponent::ApplyActivePaintCanvasToFaceDecal(
    UMaterialInterface* FaceDecalMaterial,
    FName AttachSocketName,
    const FTransform& FaceDecalTransformOffset,
    FVector FaceDecalSize,
    FName TextureParameterName)
{
    if (!ApplyLocalPaintCanvasToFaceDecal(
       FaceDecalMaterial,
       AttachSocketName,
       FaceDecalTransformOffset,
       FaceDecalSize,
       TextureParameterName))
    {
       return false;
    }

    CacheLocalPaintCanvasFaceDecalForTravel(
       FaceDecalMaterial,
       AttachSocketName,
       FaceDecalTransformOffset,
       FaceDecalSize,
       TextureParameterName);
    SubmitPaintCanvasFaceDecalForNetwork(
       FaceDecalMaterial,
       AttachSocketName,
       FaceDecalTransformOffset,
       FaceDecalSize,
       TextureParameterName);
    return true;
}

bool UPaintCanvasComponent::ApplyLocalPaintCanvasToFaceDecal(
    UMaterialInterface* FaceDecalMaterial,
    FName AttachSocketName,
    const FTransform& FaceDecalTransformOffset,
    FVector FaceDecalSize,
    FName TextureParameterName)
{
    UTextureRenderTarget2D* PaintRenderTarget = GetActivePaintCanvasRenderTarget();
    UPaintCanvasDisplay* PaintPresentation = GetOrCreatePresentation();
    if (!PaintRenderTarget || !FaceDecalMaterial || !PaintPresentation)
    {
        return false;
    }

    // 기존 구현처럼 새 Decal 생성을 시작하면 이전 Decal부터 제거한다.
    PaintPresentation->ClearFaceDecal();

    UTextureRenderTarget2D* PaintSnapshot = CreatePaintCanvasCopy(PaintRenderTarget, 1.0, FLinearColor::Transparent);
    if (!PaintSnapshot)
    {
        return false;
    }

    return PaintPresentation->ApplyFaceDecal(
            PaintSnapshot,
            FaceDecalMaterial,
            AttachSocketName,
            FaceDecalTransformOffset,
            FaceDecalSize,
            TextureParameterName);
}

void UPaintCanvasComponent::SubmitPaintCanvasFaceDecalForNetwork(
    UMaterialInterface* FaceDecalMaterial,
    FName AttachSocketName,
    const FTransform& FaceDecalTransformOffset,
    FVector FaceDecalSize,
    FName TextureParameterName)
{
    const APdPlayer* PlayerOwner = GetPlayerOwner();
    if (PlayerOwner && PlayerOwner->GetNetMode() != NM_Standalone)
    {
        ServerApplyPaintCanvasFaceDecal(
            LocalPaintStrokes,
            FaceDecalMaterial,
            AttachSocketName,
            FaceDecalTransformOffset,
            FaceDecalSize,
            TextureParameterName);
    }
}

void UPaintCanvasComponent::ServerExportPaintCanvas_Implementation(const TArray<FPaintCanvasStroke>& Strokes)
{
    if (!IsValidPaintCanvasStrokes(Strokes)
        || !TryConsumePaintNetworkEvent(LastPaintExportServerTime, PaintControlNetworkMinInterval))
    {
        return;
    }

    MulticastExportPaintCanvas(Strokes);
}

void UPaintCanvasComponent::MulticastExportPaintCanvas_Implementation(const TArray<FPaintCanvasStroke>& Strokes)
{
    const APdPlayer* PlayerOwner = GetPlayerOwner();
    if (!PlayerOwner || PlayerOwner->GetNetMode() == NM_DedicatedServer || PlayerOwner->IsLocallyControlled())
    {
        return;
    }

    if (ReplayPaintCanvasStrokes(Strokes))
    {
        StartLocalPaintCanvasExport();
    }
}

void UPaintCanvasComponent::ServerApplyPaintCanvasFaceDecal_Implementation(
    const TArray<FPaintCanvasStroke>& Strokes,
    UMaterialInterface* FaceDecalMaterial,
    FName AttachSocketName,
    FTransform FaceDecalTransformOffset,
    FVector FaceDecalSize,
    FName TextureParameterName)
{
    if (!IsValidPaintCanvasStrokes(Strokes)
        || !PaintCanvasSync::IsValidFaceDecalPayload(
            FaceDecalMaterial,
            FaceDecalTransformOffset,
            FaceDecalSize,
            MaxReplicatedFaceDecalSize,
            MaxReplicatedFaceDecalOffsetDistance,
            MaxReplicatedPaintTransformScale)
        || !TryConsumePaintNetworkEvent(LastPaintFaceDecalServerTime, PaintControlNetworkMinInterval))
    {
        return;
    }

    MulticastApplyPaintCanvasFaceDecal(
        Strokes,
        FaceDecalMaterial,
        AttachSocketName,
        FaceDecalTransformOffset,
        FaceDecalSize,
        TextureParameterName);
}

void UPaintCanvasComponent::MulticastApplyPaintCanvasFaceDecal_Implementation(
    const TArray<FPaintCanvasStroke>& Strokes,
    UMaterialInterface* FaceDecalMaterial,
    FName AttachSocketName,
    FTransform FaceDecalTransformOffset,
    FVector FaceDecalSize,
    FName TextureParameterName)
{
    const APdPlayer* PlayerOwner = GetPlayerOwner();
    if (!PlayerOwner || PlayerOwner->GetNetMode() == NM_DedicatedServer || PlayerOwner->IsLocallyControlled())
    {
        return;
    }

    if (ReplayPaintCanvasStrokes(Strokes))
    {
        ApplyLocalPaintCanvasToFaceDecal(
            FaceDecalMaterial,
            AttachSocketName,
            FaceDecalTransformOffset,
            FaceDecalSize,
            TextureParameterName);
    }
}

void UPaintCanvasComponent::RestoreCachedLobbyPaintCanvasFaceDecal()
{
    APdPlayer* PlayerOwner = GetPlayerOwner();
    if (!PlayerOwner || !PlayerOwner->IsLocallyControlled())
    {
        return;
    }

    ULobbyRuntimeSubsystem* LobbySubsystem =
        UGameInstance::GetSubsystem<ULobbyRuntimeSubsystem>(PlayerOwner->GetGameInstance());
    if (!LobbySubsystem)
    {
        return;
    }

    FLobbyPaintCanvasFaceDecalCache FaceDecalCache;
    if (!LobbySubsystem->ConsumeLocalLobbyPaintCanvasFaceDecalCache(FaceDecalCache))
    {
        return;
    }

    CancelPaintCanvasExport();
    LocalPaintStrokes.Reset();
    const int32 StrokeCount = FMath::Min(
        FaceDecalCache.Strokes.Num(),
        FMath::Clamp(MaxReplicatedPaintStrokeHistory, 1, 8192));
    for (int32 StrokeIndex = 0; StrokeIndex < StrokeCount; ++StrokeIndex)
    {
        const FLobbyPaintCanvasStrokeCache& CachedStroke = FaceDecalCache.Strokes[StrokeIndex];
        FPaintCanvasStroke& Stroke = LocalPaintStrokes.AddDefaulted_GetRef();
        Stroke.BrushSize = static_cast<float>(
            PaintCanvasSync::ClampBrushSize(CachedStroke.BrushSize, MaxReplicatedPaintBrushSize));
        Stroke.DrawLocation = FVector2f(CachedStroke.DrawLocation);
        Stroke.bStartsNewStroke = CachedStroke.bStartsNewStroke;
    }

    if (IsValidPaintCanvasStrokes(LocalPaintStrokes) && ReplayPaintCanvasStrokes(LocalPaintStrokes))
    {
        ApplyActivePaintCanvasToFaceDecal(
            FaceDecalCache.FaceDecalMaterial,
            FaceDecalCache.AttachSocketName,
            FaceDecalCache.FaceDecalTransformOffset,
            FaceDecalCache.FaceDecalSize,
            FaceDecalCache.TextureParameterName);
    }
}

APdPlayer* UPaintCanvasComponent::GetPlayerOwner() const
{
    return Cast<APdPlayer>(GetOwner());
}
