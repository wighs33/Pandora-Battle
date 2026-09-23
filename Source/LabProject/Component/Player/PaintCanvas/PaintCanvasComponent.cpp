#include "Component/Player/PaintCanvas/PaintCanvasComponent.h"

#include "Character/PdPlayer.h"
#include "Component/Player/PaintCanvas/PaintCanvasDisplay.h"
#include "Definition/UI/WidgetClassDefinition.h"
#include "Engine/Canvas.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Lobby/LobbyRuntimeSubsystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PaintCanvasComponent)

DEFINE_LOG_CATEGORY_STATIC(PdPaintCanvasComponentLog, Log, All);

namespace
{
    const FLinearColor OpaqueBlackBrushColor(0.0f, 0.0f, 0.0f, 1.0f);
}

void FReplicatedPaintCanvasStrokeArray::PostReplicatedAdd(
    const TArrayView<int32>& ,
    const int32 )
{
    if (Owner)
    {
       Owner->SchedulePaintStateReconciliation();
    }
}

void FReplicatedPaintCanvasStrokeArray::PostReplicatedChange(
    const TArrayView<int32>& ,
    const int32 )
{
    if (Owner)
    {
       Owner->SchedulePaintStateReconciliation();
    }
}

void FReplicatedPaintCanvasStrokeArray::PreReplicatedRemove(
    const TArrayView<int32>& ,
    const int32 )
{
    if (Owner)
    {
       Owner->SchedulePaintStateReconciliation();
    }
}

UPaintCanvasComponent::UPaintCanvasComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);

    ReplicatedPaintStrokes.SetOwner(this);
}

void UPaintCanvasComponent::BeginPlay()
{
    Super::BeginPlay();
    ReplicatedPaintStrokes.SetOwner(this);
    GetOrCreatePresentation();
    HidePaintSpeechBubble();
    SchedulePaintStateReconciliation();
}

void UPaintCanvasComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    CancelPaintCanvasExport();
    if (Presentation)
    {
        Presentation->Reset(PaintSpeechBubbleComponentName);
    }

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(PaintStrokeBatchTimerHandle);
        World->GetTimerManager().ClearTimer(PaintStateReconcileTimerHandle);
    }

    Super::EndPlay(EndPlayReason);
}

void UPaintCanvasComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    FDoRepLifetimeParams Params;
    Params.bIsPushBased = true;
    DOREPLIFETIME_WITH_PARAMS_FAST(
       UPaintCanvasComponent,
       ReplicatedPaintStrokes,
       Params);
    DOREPLIFETIME_WITH_PARAMS_FAST(
       UPaintCanvasComponent,
       ReplicatedPaintStateHeader,
       Params);
}

void UPaintCanvasComponent::SetSpeechBubbleComponent(
    UPrimitiveComponent* InSpeechBubbleComponent)
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

bool UPaintCanvasComponent::EnsurePaintCanvasRenderResources(
    const bool bResetCanvas)
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
       const UWidgetClassDefinition* WidgetDefinition =
          UWidgetClassDefinition::ResolveWidgetClassDefinition(this);
       UMaterialInterface* BrushMaterialParent = WidgetDefinition
                                                    ? WidgetDefinition->GetSkinWidgetSettings().PaintBrushMaterial.
                                                                        LoadSynchronous()
                                                    : nullptr;
       if (BrushMaterialParent)
       {
          PaintBrushMaterial =
             UMaterialInstanceDynamic::Create(BrushMaterialParent, this);
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
    if (PaintCanvasRenderTarget)
    {
       UKismetRenderingLibrary::ClearRenderTarget2D(
          this,
          PaintCanvasRenderTarget,
          ClearColor);
    }
}

bool UPaintCanvasComponent::DrawBrushToRenderTarget(
    UTexture2D* InBrushTexture,
    const double InBrushSize,
    const FVector2D& DrawLocation)
{
    const double SafeBrushSize = PaintCanvasSync::ClampBrushSize(
        InBrushSize,
        MaxReplicatedPaintBrushSize);
    if (!PaintCanvasRenderTarget
        || !PaintBrushMaterial
        || SafeBrushSize <= 0.0
        || !PaintCanvasSync::IsValidDrawLocation(DrawLocation))
    {
        return false;
    }

    if (InBrushTexture)
    {
        PaintBrushMaterial->SetTextureParameterValue(
            BrushTextureParameterName,
            InBrushTexture);
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
        const FVector2D BrushScreenPosition =
            RenderTargetSize * DrawLocation - BrushScreenSize * 0.5;
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

void UPaintCanvasComponent::ApplyBrushMaterialParameters() const
{
    if (!PaintBrushMaterial)
    {
       return;
    }

    PaintBrushMaterial->SetVectorParameterValue(
       BrushColorParameterName,
       OpaqueBlackBrushColor);
    PaintBrushMaterial->SetVectorParameterValue(
       TEXT("BaseColor"),
       OpaqueBlackBrushColor);
}

bool UPaintCanvasComponent::BeginPaintCanvasUiSession()
{
    CancelPaintCanvasExport();
    bPaintCanvasUiSessionActive = false;

    APdPlayer* PlayerOwner = GetPlayerOwner();
    if (!PlayerOwner || !EnsurePaintCanvasRenderResources(true))
    {
        return false;
    }

    bPaintCanvasUiSessionActive = true;
    LocalSyncState.PredictedStrokeCount = 0;
    LocalSyncState.PredictedChecksum = 0;
    PendingPaintStrokeRequests.Reset();

    if (PlayerOwner->IsLocallyControlled())
    {
        if (ULobbyRuntimeSubsystem* LobbySubsystem =
            UGameInstance::GetSubsystem<ULobbyRuntimeSubsystem>(
                PlayerOwner->GetGameInstance()))
        {
            LobbySubsystem->ResetLocalLobbyPaintCanvasCache();
        }
    }

    SubmitPaintCanvasResetForNetwork();
    return true;
}

bool UPaintCanvasComponent::PaintAtNormalizedLocation(
    const FVector2D& DrawLocation)
{
    if (!bPaintCanvasUiSessionActive
        || LocalSyncState.PredictedStrokeCount
            >= FMath::Max(MaxReplicatedPaintStrokeHistory, 1)
        || !PaintCanvasSync::IsValidDrawLocation(DrawLocation))
    {
        return false;
    }

    UTexture2D* PaintBrushTexture = BrushTexture.Get();
    const float PaintBrushSize = static_cast<float>(
        PaintCanvasSync::ClampBrushSize(
            BrushSize,
            MaxReplicatedPaintBrushSize));
    const FVector2f QuantizedDrawLocation(DrawLocation);
    const FVector2D SharedDrawLocation(QuantizedDrawLocation);
    const bool bPainted = DrawBrushToRenderTarget(
        PaintBrushTexture,
        PaintBrushSize,
        SharedDrawLocation);

    if (bPainted)
    {
        ++LocalSyncState.PredictedStrokeCount;

        FReplicatedPaintCanvasStroke PredictedStroke;
        PredictedStroke.Sequence = static_cast<uint32>(
            LocalSyncState.PredictedStrokeCount);
        PredictedStroke.BrushSize = PaintBrushSize;
        PredictedStroke.DrawLocation = QuantizedDrawLocation;

        LocalSyncState.PredictedChecksum =
            PaintCanvasSync::AccumulateStrokeChecksum(
                LocalSyncState.PredictedChecksum,
                PredictedStroke);

        CacheLocalPaintCanvasStrokeForTravel(
            PaintBrushTexture,
            PaintBrushSize,
            SharedDrawLocation);
        SubmitPaintCanvasStrokeForNetwork(
            PaintBrushSize,
            SharedDrawLocation);
    }

    return bPainted;
}

void UPaintCanvasComponent::HidePaintCanvas()
{
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
    UTextureRenderTarget2D* PaintRenderTarget =
        GetActivePaintCanvasRenderTarget();
    if (!PaintRenderTarget)
    {
        UE_LOG(
            PdPaintCanvasComponentLog,
            Warning,
            TEXT("Failed to export paint canvas to speech bubble. RenderTarget=None"));
        return false;
    }

    UTextureRenderTarget2D* SpeechBubbleRenderTarget =
        CreatePaintCanvasCopy(
            PaintRenderTarget,
            PaintSpeechBubbleRenderTargetScale,
            FLinearColor::White);
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

void UPaintCanvasComponent::ResetLocalSharedPaintCanvas()
{
    CancelPaintCanvasExport();
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(PaintStrokeBatchTimerHandle);
    }

    PendingPaintStrokeRequests.Reset();
    bHasPendingPaintExport = false;
    bHasPendingFaceDecal = false;
    PendingFaceDecalMaterial = nullptr;
    EnsurePaintCanvasRenderResources(true);
    LocalSyncState.Reset();
}

void UPaintCanvasComponent::SubmitPaintCanvasResetForNetwork()
{
    APdPlayer* PlayerOwner = GetPlayerOwner();
    if (!PlayerOwner || PlayerOwner->GetNetMode() == NM_Standalone)
    {
       return;
    }

    if (PlayerOwner->HasAuthority())
    {
       ResetAuthoritativePaintCanvas();
       return;
    }

    ServerResetSharedPaintCanvas();
}

void UPaintCanvasComponent::SubmitPaintCanvasStrokeForNetwork(
    const double InBrushSize,
    const FVector2D& DrawLocation)
{
    APdPlayer* PlayerOwner = GetPlayerOwner();
    const double SafeBrushSize = PaintCanvasSync::ClampBrushSize(
        InBrushSize,
        MaxReplicatedPaintBrushSize);
    if (!PlayerOwner
        || PlayerOwner->GetNetMode() == NM_Standalone
        || SafeBrushSize <= 0.0
        || !PaintCanvasSync::IsValidDrawLocation(DrawLocation))
    {
        return;
    }

    FPaintCanvasStrokeRequest& Request =
        PendingPaintStrokeRequests.AddDefaulted_GetRef();
    Request.BrushSize = static_cast<float>(SafeBrushSize);
    Request.DrawLocation = FVector2f(DrawLocation);

    if (PendingPaintStrokeRequests.Num()
        >= FMath::Clamp(MaxPaintStrokesPerBatch, 1, 64))
    {
        FlushPendingPaintStrokeBatches(false);
    }
    else
    {
        SchedulePendingPaintStrokeFlush();
    }
}

void UPaintCanvasComponent::SchedulePendingPaintStrokeFlush()
{
    UWorld* World = GetWorld();
    if (!World
       || PendingPaintStrokeRequests.IsEmpty()
       || World->GetTimerManager().IsTimerActive(
          PaintStrokeBatchTimerHandle))
    {
       return;
    }

    FTimerDelegate FlushDelegate;
    FlushDelegate.BindUObject(
       this,
       &ThisClass::FlushPendingPaintStrokeBatches,
       false);
    World->GetTimerManager().SetTimer(
       PaintStrokeBatchTimerHandle,
       FlushDelegate,
       static_cast<float>(FMath::Max(PaintStrokeBatchInterval, 0.005)),
       false);
}

void UPaintCanvasComponent::FlushPendingPaintStrokeBatches(
    const bool bFlushAll)
{
    if (UWorld* World = GetWorld())
    {
       World->GetTimerManager().ClearTimer(PaintStrokeBatchTimerHandle);
    }

    APdPlayer* PlayerOwner = GetPlayerOwner();
    if (!PlayerOwner || PlayerOwner->GetNetMode() == NM_Standalone)
    {
       PendingPaintStrokeRequests.Reset();
       return;
    }

    const int32 SafeBatchSize =
       FMath::Clamp(MaxPaintStrokesPerBatch, 1, 64);
    do
    {
       const int32 StrokeCount = FMath::Min(
          SafeBatchSize,
          PendingPaintStrokeRequests.Num());
       if (StrokeCount <= 0)
       {
          break;
       }

       TArray<FPaintCanvasStrokeRequest> StrokeBatch;
       StrokeBatch.Append(
          PendingPaintStrokeRequests.GetData(),
          StrokeCount);
       PendingPaintStrokeRequests.RemoveAt(
          0,
          StrokeCount,
          EAllowShrinking::No);

       if (PlayerOwner->HasAuthority())
       {
          CommitAuthoritativePaintStrokeBatch(StrokeBatch);
       }
       else
       {
          ServerSubmitPaintCanvasStrokeBatch(StrokeBatch);
       }
    }
    while (bFlushAll && !PendingPaintStrokeRequests.IsEmpty());

    if (!PendingPaintStrokeRequests.IsEmpty())
    {
       SchedulePendingPaintStrokeFlush();
    }
}

void UPaintCanvasComponent::CommitAuthoritativePaintStrokeBatch(
    const TArray<FPaintCanvasStrokeRequest>& StrokeBatch)
{
    APdPlayer* PlayerOwner = GetPlayerOwner();
    const int32 SafeMaxHistory =
        FMath::Clamp(MaxReplicatedPaintStrokeHistory, 1, 8192);
    const int32 SafeBatchSize =
        FMath::Clamp(MaxPaintStrokesPerBatch, 1, 64);
    if (!PlayerOwner
        || !PlayerOwner->HasAuthority()
        || StrokeBatch.IsEmpty()
        || StrokeBatch.Num() > SafeBatchSize
        || ReplicatedPaintStrokes.Items.Num() + StrokeBatch.Num()
            > SafeMaxHistory)
    {
        return;
    }

    for (const FPaintCanvasStrokeRequest& Request : StrokeBatch)
    {
        const FVector2D DrawLocation(Request.DrawLocation);
        if (PaintCanvasSync::ClampBrushSize(
                Request.BrushSize,
                MaxReplicatedPaintBrushSize) <= 0.0
            || !PaintCanvasSync::IsValidDrawLocation(DrawLocation))
        {
            return;
        }
    }

    for (const FPaintCanvasStrokeRequest& Request : StrokeBatch)
    {
        FReplicatedPaintCanvasStroke& Stroke =
            ReplicatedPaintStrokes.Items.AddDefaulted_GetRef();
        Stroke.Sequence = ++ReplicatedPaintStateHeader.LastSequence;
        Stroke.BrushSize = static_cast<float>(
            PaintCanvasSync::ClampBrushSize(
                Request.BrushSize,
                MaxReplicatedPaintBrushSize));
        Stroke.DrawLocation = Request.DrawLocation;
        ReplicatedPaintStateHeader.Checksum =
            PaintCanvasSync::AccumulateStrokeChecksum(
                ReplicatedPaintStateHeader.Checksum,
                Stroke);
        ReplicatedPaintStrokes.MarkItemDirty(Stroke);
    }

    MARK_PROPERTY_DIRTY_FROM_NAME(
        UPaintCanvasComponent,
        ReplicatedPaintStrokes,
        this);
    MARK_PROPERTY_DIRTY_FROM_NAME(
        UPaintCanvasComponent,
        ReplicatedPaintStateHeader,
        this);
    PlayerOwner->ForceNetUpdate();

    if (PlayerOwner->GetNetMode() != NM_DedicatedServer)
    {
        SchedulePaintStateReconciliation();
    }
}

void UPaintCanvasComponent::ResetAuthoritativePaintCanvas()
{
    APdPlayer* PlayerOwner = GetPlayerOwner();
    if (!PlayerOwner || !PlayerOwner->HasAuthority())
    {
        return;
    }

    ReplicatedPaintStrokes.Items.Reset();
    ReplicatedPaintStrokes.MarkArrayDirty();
    ReplicatedPaintStateHeader.Revision =
        ReplicatedPaintStateHeader.Revision == MAX_uint32
            ? 1
            : ReplicatedPaintStateHeader.Revision + 1;
    ReplicatedPaintStateHeader.LastSequence = 0;
    ReplicatedPaintStateHeader.Checksum = 0;

    MARK_PROPERTY_DIRTY_FROM_NAME(
        UPaintCanvasComponent,
        ReplicatedPaintStrokes,
        this);
    MARK_PROPERTY_DIRTY_FROM_NAME(
        UPaintCanvasComponent,
        ReplicatedPaintStateHeader,
        this);
    PlayerOwner->ForceNetUpdate();

    LocalSyncState.ResetAppliedToAuthoritative(ReplicatedPaintStateHeader);
    if (PlayerOwner->GetNetMode() != NM_DedicatedServer)
    {
        EnsurePaintCanvasRenderResources(true);
    }
}

void UPaintCanvasComponent::OnRep_PaintCanvasStateHeader()
{
    SchedulePaintStateReconciliation();
}

void UPaintCanvasComponent::SchedulePaintStateReconciliation()
{
    UWorld* World = GetWorld();
    if (!World
       || World->GetTimerManager().IsTimerActive(
          PaintStateReconcileTimerHandle))
    {
       return;
    }

    PaintStateReconcileTimerHandle =
       World->GetTimerManager().SetTimerForNextTick(
          this,
          &ThisClass::ReconcileReplicatedPaintState);
}

void UPaintCanvasComponent::RebuildCanvasFromAuthoritativeHistory(
    const TArray<const FReplicatedPaintCanvasStroke*>& SortedStrokes)
{
    if (!EnsurePaintCanvasRenderResources(true))
    {
       return;
    }

    for (const FReplicatedPaintCanvasStroke* Stroke : SortedStrokes)
    {
       if (Stroke)
       {
          DrawBrushToRenderTarget(
             BrushTexture.Get(),
             Stroke->BrushSize,
             FVector2D(Stroke->DrawLocation));
       }
    }
}

void UPaintCanvasComponent::ReconcileReplicatedPaintState()
{
    PaintStateReconcileTimerHandle.Invalidate();

    const APdPlayer* PlayerOwner = GetPlayerOwner();
    if (!PlayerOwner
        || PlayerOwner->GetNetMode() == NM_DedicatedServer
        || PlayerOwner->GetNetMode() == NM_Standalone)
    {
        return;
    }

    TArray<const FReplicatedPaintCanvasStroke*> SortedStrokes;
    SortedStrokes.Reserve(ReplicatedPaintStrokes.Items.Num());
    for (const FReplicatedPaintCanvasStroke& Stroke : ReplicatedPaintStrokes.Items)
    {
        SortedStrokes.Add(&Stroke);
    }

    uint32 ValidatedChecksum = 0;
    if (!PaintCanvasSync::ValidateAuthoritativeHistory(
            SortedStrokes,
            ReplicatedPaintStateHeader,
            MaxReplicatedPaintStrokeHistory,
            ValidatedChecksum))
    {
        return;
    }

    if (LocalSyncState.IsAppliedStateSynchronized(
            ReplicatedPaintStateHeader.Revision,
            ReplicatedPaintStateHeader.LastSequence,
            ValidatedChecksum))
    {
        ProcessPendingPaintPresentation();
        return;
    }

    const bool bCanAcceptLocalPrediction =
        PlayerOwner->IsLocallyControlled()
        && PaintCanvasRenderTarget
        && LocalSyncState.PredictedStrokeCount
            == static_cast<int32>(ReplicatedPaintStateHeader.LastSequence)
        && LocalSyncState.PredictedChecksum == ValidatedChecksum;

    if (!bCanAcceptLocalPrediction)
    {
        bool bAppliedIncrementally = false;
        if (LocalSyncState.AppliedRevision
                == ReplicatedPaintStateHeader.Revision
            && LocalSyncState.AppliedLastSequence
                <= ReplicatedPaintStateHeader.LastSequence
            && EnsurePaintCanvasRenderResources())
        {
            uint32 RunningChecksum = LocalSyncState.AppliedChecksum;
            for (const FReplicatedPaintCanvasStroke* Stroke : SortedStrokes)
            {
                if (Stroke
                    && Stroke->Sequence > LocalSyncState.AppliedLastSequence)
                {
                    DrawBrushToRenderTarget(
                        BrushTexture.Get(),
                        Stroke->BrushSize,
                        FVector2D(Stroke->DrawLocation));
                    RunningChecksum =
                        PaintCanvasSync::AccumulateStrokeChecksum(
                            RunningChecksum,
                            *Stroke);
                }
            }

            bAppliedIncrementally = RunningChecksum == ValidatedChecksum;
        }

        if (!bAppliedIncrementally)
        {
            RebuildCanvasFromAuthoritativeHistory(SortedStrokes);
        }
    }

    LocalSyncState.AcceptAuthoritative(
        ReplicatedPaintStateHeader,
        ValidatedChecksum);
    ProcessPendingPaintPresentation();
}

void UPaintCanvasComponent::ProcessPendingPaintPresentation()
{
    if (bHasPendingPaintExport
        && LocalSyncState.IsAppliedStateSynchronized(
            PendingPaintExportRevision,
            PendingPaintExportLastSequence,
            PendingPaintExportChecksum))
    {
        bHasPendingPaintExport = false;
        StartLocalPaintCanvasExport();
    }

    if (bHasPendingFaceDecal
        && LocalSyncState.IsAppliedStateSynchronized(
            PendingFaceDecalRevision,
            PendingFaceDecalLastSequence,
            PendingFaceDecalChecksum))
    {
        UMaterialInterface* FaceDecalMaterial = PendingFaceDecalMaterial.Get();
        const FName AttachSocketName = PendingFaceDecalAttachSocketName;
        const FTransform TransformOffset = PendingFaceDecalTransformOffset;
        const FVector FaceDecalSize = PendingFaceDecalSize;
        const FName TextureParameterName =
            PendingFaceDecalTextureParameterName;

        bHasPendingFaceDecal = false;
        PendingFaceDecalMaterial = nullptr;
        ApplyLocalPaintCanvasToFaceDecal(
            FaceDecalMaterial,
            AttachSocketName,
            TransformOffset,
            FaceDecalSize,
            TextureParameterName);
    }
}

void UPaintCanvasComponent::SubmitPaintCanvasExportForNetwork()
{
    APdPlayer* PlayerOwner = GetPlayerOwner();
    if (!PlayerOwner
       || PlayerOwner->GetNetMode() == NM_Standalone)
    {
       return;
    }

    FlushPendingPaintStrokeBatches(true);

    if (PlayerOwner->HasAuthority())
    {
       if (!TryConsumePaintNetworkEvent(LastPaintExportServerTime, PaintControlNetworkMinInterval))
       {
          return;
       }

       MulticastExportPaintCanvas(
          ReplicatedPaintStateHeader.Revision,
          ReplicatedPaintStateHeader.LastSequence,
          ReplicatedPaintStateHeader.Checksum);
       return;
    }

    ServerExportPaintCanvas();
}

void UPaintCanvasComponent::CacheLocalPaintCanvasStrokeForTravel(
    UTexture2D* InBrushTexture,
    const double InBrushSize,
    const FVector2D& DrawLocation) const
{
    const APdPlayer* PlayerOwner = GetPlayerOwner();
    if (!PlayerOwner || !PlayerOwner->IsLocallyControlled())
    {
       return;
    }

    if (ULobbyRuntimeSubsystem* LobbySubsystem = UGameInstance::GetSubsystem<ULobbyRuntimeSubsystem>(
       PlayerOwner->GetGameInstance()))
    {
       LobbySubsystem->CacheLocalLobbyPaintCanvasStroke(InBrushTexture, InBrushSize, DrawLocation);
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
    UTextureRenderTarget2D* PaintRenderTarget =
        GetActivePaintCanvasRenderTarget();
    UPaintCanvasDisplay* PaintPresentation = GetOrCreatePresentation();
    if (!PaintRenderTarget || !FaceDecalMaterial || !PaintPresentation)
    {
        return false;
    }

    // 기존 구현처럼 새 Decal 생성을 시작하면 이전 Decal부터 제거한다.
    PaintPresentation->ClearFaceDecal();

    UTextureRenderTarget2D* PaintSnapshot = CreatePaintCanvasCopy(
        PaintRenderTarget,
        1.0,
        FLinearColor::Transparent);
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
    APdPlayer* PlayerOwner = GetPlayerOwner();
    if (!PlayerOwner
        || PlayerOwner->GetNetMode() == NM_Standalone
        || !PaintCanvasSync::IsValidFaceDecalPayload(
            FaceDecalMaterial,
            FaceDecalTransformOffset,
            FaceDecalSize,
            MaxReplicatedFaceDecalSize,
            MaxReplicatedFaceDecalOffsetDistance,
            MaxReplicatedPaintTransformScale))
    {
        return;
    }

    FlushPendingPaintStrokeBatches(true);

    if (PlayerOwner->HasAuthority())
    {
        if (!TryConsumePaintNetworkEvent(
            LastPaintFaceDecalServerTime,
            PaintControlNetworkMinInterval))
        {
            return;
        }

        MulticastApplyPaintCanvasFaceDecal(
            FaceDecalMaterial,
            AttachSocketName,
            FaceDecalTransformOffset,
            FaceDecalSize,
            TextureParameterName,
            ReplicatedPaintStateHeader.Revision,
            ReplicatedPaintStateHeader.LastSequence,
            ReplicatedPaintStateHeader.Checksum);
        return;
    }

    ServerApplyPaintCanvasFaceDecal(
        FaceDecalMaterial,
        AttachSocketName,
        FaceDecalTransformOffset,
        FaceDecalSize,
        TextureParameterName);
}

void UPaintCanvasComponent::ServerResetSharedPaintCanvas_Implementation()
{
    ResetAuthoritativePaintCanvas();
}

void UPaintCanvasComponent::ServerSubmitPaintCanvasStrokeBatch_Implementation(
    const TArray<FPaintCanvasStrokeRequest>& StrokeBatch)
{
    CommitAuthoritativePaintStrokeBatch(StrokeBatch);
}

void UPaintCanvasComponent::ServerExportPaintCanvas_Implementation()
{
    if (!TryConsumePaintNetworkEvent(
       LastPaintExportServerTime,
       PaintControlNetworkMinInterval))
    {
       return;
    }

    MulticastExportPaintCanvas(
       ReplicatedPaintStateHeader.Revision,
       ReplicatedPaintStateHeader.LastSequence,
       ReplicatedPaintStateHeader.Checksum);
}

void UPaintCanvasComponent::MulticastExportPaintCanvas_Implementation(
    const uint32 RequiredRevision,
    const uint32 RequiredLastSequence,
    const uint32 RequiredChecksum)
{
    const APdPlayer* PlayerOwner = GetPlayerOwner();
    if (!PlayerOwner
        || PlayerOwner->GetNetMode() == NM_DedicatedServer
        || PlayerOwner->IsLocallyControlled())
    {
        return;
    }

    if (LocalSyncState.IsAppliedStateSynchronized(
        RequiredRevision,
        RequiredLastSequence,
        RequiredChecksum))
    {
        StartLocalPaintCanvasExport();
        return;
    }

    bHasPendingPaintExport = true;
    PendingPaintExportRevision = RequiredRevision;
    PendingPaintExportLastSequence = RequiredLastSequence;
    PendingPaintExportChecksum = RequiredChecksum;
    SchedulePaintStateReconciliation();
}

void UPaintCanvasComponent::ServerApplyPaintCanvasFaceDecal_Implementation(
    UMaterialInterface* FaceDecalMaterial,
    FName AttachSocketName,
    FTransform FaceDecalTransformOffset,
    FVector FaceDecalSize,
    FName TextureParameterName)
{
    if (!PaintCanvasSync::IsValidFaceDecalPayload(
            FaceDecalMaterial,
            FaceDecalTransformOffset,
            FaceDecalSize,
            MaxReplicatedFaceDecalSize,
            MaxReplicatedFaceDecalOffsetDistance,
            MaxReplicatedPaintTransformScale)
        || !TryConsumePaintNetworkEvent(
            LastPaintFaceDecalServerTime,
            PaintControlNetworkMinInterval))
    {
        return;
    }

    MulticastApplyPaintCanvasFaceDecal(
        FaceDecalMaterial,
        AttachSocketName,
        FaceDecalTransformOffset,
        FaceDecalSize,
        TextureParameterName,
        ReplicatedPaintStateHeader.Revision,
        ReplicatedPaintStateHeader.LastSequence,
        ReplicatedPaintStateHeader.Checksum);
}

void UPaintCanvasComponent::MulticastApplyPaintCanvasFaceDecal_Implementation(
    UMaterialInterface* FaceDecalMaterial,
    FName AttachSocketName,
    FTransform FaceDecalTransformOffset,
    FVector FaceDecalSize,
    FName TextureParameterName,
    const uint32 RequiredRevision,
    const uint32 RequiredLastSequence,
    const uint32 RequiredChecksum)
{
    const APdPlayer* PlayerOwner = GetPlayerOwner();
    if (!PlayerOwner
        || PlayerOwner->GetNetMode() == NM_DedicatedServer
        || PlayerOwner->IsLocallyControlled()
        || !PaintCanvasSync::IsValidFaceDecalPayload(
            FaceDecalMaterial,
            FaceDecalTransformOffset,
            FaceDecalSize,
            MaxReplicatedFaceDecalSize,
            MaxReplicatedFaceDecalOffsetDistance,
            MaxReplicatedPaintTransformScale))
    {
        return;
    }

    if (LocalSyncState.IsAppliedStateSynchronized(
        RequiredRevision,
        RequiredLastSequence,
        RequiredChecksum))
    {
        ApplyLocalPaintCanvasToFaceDecal(
            FaceDecalMaterial,
            AttachSocketName,
            FaceDecalTransformOffset,
            FaceDecalSize,
            TextureParameterName);
        return;
    }

    bHasPendingFaceDecal = true;
    PendingFaceDecalMaterial = FaceDecalMaterial;
    PendingFaceDecalAttachSocketName = AttachSocketName;
    PendingFaceDecalTransformOffset = FaceDecalTransformOffset;
    PendingFaceDecalSize = FaceDecalSize;
    PendingFaceDecalTextureParameterName = TextureParameterName;
    PendingFaceDecalRevision = RequiredRevision;
    PendingFaceDecalLastSequence = RequiredLastSequence;
    PendingFaceDecalChecksum = RequiredChecksum;
    SchedulePaintStateReconciliation();
}

void UPaintCanvasComponent::RestoreCachedLobbyPaintCanvasFaceDecal()
{
    APdPlayer* PlayerOwner = GetPlayerOwner();
    if (!PlayerOwner || !PlayerOwner->IsLocallyControlled())
    {
        return;
    }

    ULobbyRuntimeSubsystem* LobbySubsystem =
        UGameInstance::GetSubsystem<ULobbyRuntimeSubsystem>(
            PlayerOwner->GetGameInstance());
    if (!LobbySubsystem)
    {
        return;
    }

    FLobbyPaintCanvasFaceDecalCache FaceDecalCache;
    if (!LobbySubsystem->ConsumeLocalLobbyPaintCanvasFaceDecalCache(
            FaceDecalCache))
    {
        return;
    }

    ResetLocalSharedPaintCanvas();
    SubmitPaintCanvasResetForNetwork();
    if (!EnsurePaintCanvasRenderResources())
    {
        return;
    }

    const int32 SafeMaxHistory =
        FMath::Clamp(MaxReplicatedPaintStrokeHistory, 1, 8192);
    for (int32 StrokeIndex = 0;
         StrokeIndex < FaceDecalCache.Strokes.Num()
            && LocalSyncState.PredictedStrokeCount < SafeMaxHistory;
         ++StrokeIndex)
    {
        const FLobbyPaintCanvasStrokeCache& Stroke =
            FaceDecalCache.Strokes[StrokeIndex];
        if (Stroke.BrushSize <= 0.0)
        {
            continue;
        }

        const float SafeBrushSize = static_cast<float>(
            PaintCanvasSync::ClampBrushSize(
                Stroke.BrushSize,
                MaxReplicatedPaintBrushSize));
        const FVector2f QuantizedDrawLocation(Stroke.DrawLocation);
        const FVector2D SharedDrawLocation(QuantizedDrawLocation);
        if (SafeBrushSize <= 0.0
            || !PaintCanvasSync::IsValidDrawLocation(SharedDrawLocation)
            || !DrawBrushToRenderTarget(
                BrushTexture.Get(),
                SafeBrushSize,
                SharedDrawLocation))
        {
            continue;
        }

        ++LocalSyncState.PredictedStrokeCount;
        FReplicatedPaintCanvasStroke PredictedStroke;
        PredictedStroke.Sequence = static_cast<uint32>(
            LocalSyncState.PredictedStrokeCount);
        PredictedStroke.BrushSize = SafeBrushSize;
        PredictedStroke.DrawLocation = QuantizedDrawLocation;
        LocalSyncState.PredictedChecksum =
            PaintCanvasSync::AccumulateStrokeChecksum(
                LocalSyncState.PredictedChecksum,
                PredictedStroke);

        SubmitPaintCanvasStrokeForNetwork(
            SafeBrushSize,
            SharedDrawLocation);
    }

    FlushPendingPaintStrokeBatches(true);

    if (!ApplyLocalPaintCanvasToFaceDecal(
        FaceDecalCache.FaceDecalMaterial,
        FaceDecalCache.AttachSocketName,
        FaceDecalCache.FaceDecalTransformOffset,
        FaceDecalCache.FaceDecalSize,
        FaceDecalCache.TextureParameterName))
    {
        return;
    }

    SubmitPaintCanvasFaceDecalForNetwork(
        FaceDecalCache.FaceDecalMaterial,
        FaceDecalCache.AttachSocketName,
        FaceDecalCache.FaceDecalTransformOffset,
        FaceDecalCache.FaceDecalSize,
        FaceDecalCache.TextureParameterName);
}

APdPlayer* UPaintCanvasComponent::GetPlayerOwner() const
{
    return Cast<APdPlayer>(GetOwner());
}