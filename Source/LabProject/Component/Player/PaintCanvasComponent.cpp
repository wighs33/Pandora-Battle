#include "Component/Player/PaintCanvasComponent.h"

#include "Character/PdPlayer.h"
#include "Components/DecalComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Definition/UI/WidgetClassDefinition.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Mode/PdGameInstance.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PaintCanvasComponent)

DEFINE_LOG_CATEGORY_STATIC(PdPaintCanvasComponentLog, Log, All);

namespace
{
	const FLinearColor OpaqueBlackBrushColor(0.0f, 0.0f, 0.0f, 1.0f);
}

void FReplicatedPaintCanvasStrokeArray::PostReplicatedAdd(
	const TArrayView<int32>& AddedIndices,
	const int32 FinalSize)
{
	static_cast<void>(AddedIndices);
	static_cast<void>(FinalSize);
	if (Owner)
	{
		Owner->HandleReplicatedPaintStateChanged();
	}
}

void FReplicatedPaintCanvasStrokeArray::PostReplicatedChange(
	const TArrayView<int32>& ChangedIndices,
	const int32 FinalSize)
{
	static_cast<void>(ChangedIndices);
	static_cast<void>(FinalSize);
	if (Owner)
	{
		Owner->HandleReplicatedPaintStateChanged();
	}
}

void FReplicatedPaintCanvasStrokeArray::PreReplicatedRemove(
	const TArrayView<int32>& RemovedIndices,
	const int32 FinalSize)
{
	static_cast<void>(RemovedIndices);
	static_cast<void>(FinalSize);
	if (Owner)
	{
		Owner->HandleReplicatedPaintStateChanged();
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
	HidePaintSpeechBubble();
	SchedulePaintStateReconciliation();
}

void UPaintCanvasComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelPaintCanvasExport();
	ClearPaintCanvasFaceDecal();
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

void UPaintCanvasComponent::SetSpeechBubbleComponent(UPrimitiveComponent* InSpeechBubbleComponent)
{
	SpeechBubbleComponent = InSpeechBubbleComponent;
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
			? WidgetDefinition->GetSkinWidgetSettings().PaintBrushMaterial.LoadSynchronous()
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
	const double SafeBrushSize =
		GetClampedReplicatedPaintBrushSize(InBrushSize);
	if (!PaintCanvasRenderTarget
		|| !PaintBrushMaterial
		|| SafeBrushSize <= 0.0
		|| !IsValidReplicatedDrawLocation(DrawLocation))
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
	LocalPredictedPaintStrokeCount = 0;
	LocalPredictedPaintChecksum = 0;
	PendingPaintStrokeRequests.Reset();
	if (PlayerOwner->IsLocallyControlled())
	{
		if (UPdGameInstance* PdGameInstance = PlayerOwner->GetGameInstance<UPdGameInstance>())
		{
			PdGameInstance->ResetLocalLobbyPaintCanvasCache();
		}
	}

	SubmitPaintCanvasResetForNetwork();
	return true;
}

bool UPaintCanvasComponent::PaintAtNormalizedLocation(const FVector2D& DrawLocation)
{
	if (!bPaintCanvasUiSessionActive
		|| LocalPredictedPaintStrokeCount
			>= FMath::Max(MaxReplicatedPaintStrokeHistory, 1)
		|| !IsValidReplicatedDrawLocation(DrawLocation))
	{
		return false;
	}

	UTexture2D* PaintBrushTexture = BrushTexture.Get();
	const float PaintBrushSize = static_cast<float>(
		GetClampedReplicatedPaintBrushSize(BrushSize));
	const FVector2f QuantizedDrawLocation(DrawLocation);
	const FVector2D SharedDrawLocation(QuantizedDrawLocation);
	const bool bPainted = DrawBrushToRenderTarget(
		PaintBrushTexture,
		PaintBrushSize,
		SharedDrawLocation);
	if (bPainted)
	{
		++LocalPredictedPaintStrokeCount;
		FReplicatedPaintCanvasStroke PredictedStroke;
		PredictedStroke.Sequence = static_cast<uint32>(
			LocalPredictedPaintStrokeCount);
		PredictedStroke.BrushSize = static_cast<float>(PaintBrushSize);
		PredictedStroke.DrawLocation = QuantizedDrawLocation;
		LocalPredictedPaintChecksum = AccumulatePaintStrokeChecksum(
			LocalPredictedPaintChecksum,
			PredictedStroke);
		CacheLocalPaintCanvasStrokeForTravel(
			PaintBrushTexture,
			PaintBrushSize,
			SharedDrawLocation);
		SubmitPaintCanvasStrokeForNetwork(
			PaintBrushTexture,
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

void UPaintCanvasComponent::FinishPaintCanvasExport()
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
			&ThisClass::FinishPaintCanvasExport,
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

double UPaintCanvasComponent::GetClampedReplicatedPaintBrushSize(const double InBrushSize) const
{
	if (!FMath::IsFinite(InBrushSize) || InBrushSize <= 0.0)
	{
		return 0.0;
	}

	return FMath::Clamp(InBrushSize, 1.0, FMath::Max(MaxReplicatedPaintBrushSize, 1.0));
}

bool UPaintCanvasComponent::IsValidReplicatedDrawLocation(const FVector2D& DrawLocation) const
{
	return FMath::IsFinite(DrawLocation.X)
		&& FMath::IsFinite(DrawLocation.Y)
		&& DrawLocation.X >= 0.0
		&& DrawLocation.X <= 1.0
		&& DrawLocation.Y >= 0.0
		&& DrawLocation.Y <= 1.0;
}

bool UPaintCanvasComponent::IsValidReplicatedPaintTransform(
	const FTransform& Transform,
	const double MaxTranslationDistance,
	const double MaxScale) const
{
	if (Transform.ContainsNaN())
	{
		return false;
	}

	const FVector Translation = Transform.GetTranslation();
	if (Translation.SizeSquared() > FMath::Square(FMath::Max(MaxTranslationDistance, 0.0)))
	{
		return false;
	}

	const FVector Scale = Transform.GetScale3D().GetAbs();
	const double SafeMaxScale = FMath::Max(MaxScale, 0.01);
	return Scale.X >= 0.01
		&& Scale.Y >= 0.01
		&& Scale.Z >= 0.01
		&& Scale.X <= SafeMaxScale
		&& Scale.Y <= SafeMaxScale
		&& Scale.Z <= SafeMaxScale;
}

bool UPaintCanvasComponent::IsValidReplicatedFaceDecalPayload(
	UMaterialInterface* FaceDecalMaterial,
	const FTransform& FaceDecalTransformOffset,
	const FVector& FaceDecalSize) const
{
	if (!FaceDecalMaterial || FaceDecalSize.ContainsNaN())
	{
		return false;
	}

	const double SafeMaxFaceDecalSize = FMath::Max(MaxReplicatedFaceDecalSize, 1.0);
	if (FaceDecalSize.X <= 0.0
		|| FaceDecalSize.Y <= 0.0
		|| FaceDecalSize.Z <= 0.0
		|| FaceDecalSize.X > SafeMaxFaceDecalSize
		|| FaceDecalSize.Y > SafeMaxFaceDecalSize
		|| FaceDecalSize.Z > SafeMaxFaceDecalSize)
	{
		return false;
	}

	return IsValidReplicatedPaintTransform(
		FaceDecalTransformOffset,
		MaxReplicatedFaceDecalOffsetDistance,
		MaxReplicatedPaintTransformScale);
}

UPrimitiveComponent* UPaintCanvasComponent::FindPaintSpeechBubbleComponent() const
{
	if (IsValid(SpeechBubbleComponent))
	{
		return SpeechBubbleComponent.Get();
	}

	const APdPlayer* PlayerOwner = GetPlayerOwner();
	if (!PlayerOwner)
	{
		return nullptr;
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	PlayerOwner->GetComponents<UPrimitiveComponent>(PrimitiveComponents);

	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!PrimitiveComponent)
		{
			continue;
		}

		const bool bMatchesConfiguredName =
			PrimitiveComponent->GetFName() == PaintSpeechBubbleComponentName
			|| PrimitiveComponent->GetName().StartsWith(PaintSpeechBubbleComponentName.ToString());
		if (bMatchesConfiguredName)
		{
			return PrimitiveComponent;
		}
	}
	return nullptr;
}

UTextureRenderTarget2D* UPaintCanvasComponent::CreateScaledPaintCanvasRenderTarget(UTextureRenderTarget2D* SourceRenderTarget)
{
	if (!SourceRenderTarget)
	{
		return nullptr;
	}

	UTextureRenderTarget2D* ScaledRenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(
		this,
		FMath::Max(SourceRenderTarget->SizeX, 1),
		FMath::Max(SourceRenderTarget->SizeY, 1),
		RTF_RGBA16f,
		FLinearColor::White,
		false,
		false);
	if (!ScaledRenderTarget)
	{
		return nullptr;
	}

	UCanvas* DrawCanvas = nullptr;
	FVector2D RenderTargetSize = FVector2D::ZeroVector;
	FDrawToRenderTargetContext Context;
	UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, ScaledRenderTarget, DrawCanvas, RenderTargetSize, Context);

	if (DrawCanvas)
	{
		const double SafeScale = FMath::Clamp(PaintSpeechBubbleRenderTargetScale, 0.01, 1.0);
		const FVector2D PaintSize = RenderTargetSize * SafeScale;
		const FVector2D PaintPosition = (RenderTargetSize - PaintSize) * 0.5;

		DrawCanvas->K2_DrawTexture(
			SourceRenderTarget,
			PaintPosition,
			PaintSize,
			FVector2D::ZeroVector,
			FVector2D(1.0, 1.0),
			FLinearColor::White,
			BLEND_Opaque,
			0.0f,
			FVector2D(0.5, 0.5));
	}

	UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Context);
	return ScaledRenderTarget;
}

bool UPaintCanvasComponent::ApplyPaintCanvasToSpeechBubble()
{
	UTextureRenderTarget2D* PaintRenderTarget = GetActivePaintCanvasRenderTarget();
	if (!PaintRenderTarget)
	{
		UE_LOG(PdPaintCanvasComponentLog, Warning, TEXT("Failed to export paint canvas to speech bubble. RenderTarget=None"));
		return false;
	}

	ActivePaintSpeechBubbleRenderTarget = CreateScaledPaintCanvasRenderTarget(PaintRenderTarget);
	UTextureRenderTarget2D* RenderTargetForSpeechBubble = ActivePaintSpeechBubbleRenderTarget.Get()
		? ActivePaintSpeechBubbleRenderTarget.Get()
		: PaintRenderTarget;

	UPrimitiveComponent* ResolvedSpeechBubbleComponent = FindPaintSpeechBubbleComponent();
	if (!ResolvedSpeechBubbleComponent)
	{
		UE_LOG(
			PdPaintCanvasComponentLog,
			Warning,
			TEXT("Failed to export paint canvas to speech bubble. SpeechBubbleComponent=None"));
		return false;
	}

	UMaterialInterface* ExistingMaterial = ResolvedSpeechBubbleComponent->GetMaterial(PaintSpeechBubbleMaterialIndex);
	if (!ExistingMaterial)
	{
		UE_LOG(
			PdPaintCanvasComponentLog,
			Warning,
			TEXT("Failed to export paint canvas to speech bubble. %s has no material at index %d."),
			*ResolvedSpeechBubbleComponent->GetName(),
			PaintSpeechBubbleMaterialIndex);
		return false;
	}

	ActivePaintSpeechBubbleComponent = ResolvedSpeechBubbleComponent;
	ActivePaintSpeechBubbleMaterial = Cast<UMaterialInstanceDynamic>(ExistingMaterial);
	if (!ActivePaintSpeechBubbleMaterial)
	{
		ActivePaintSpeechBubbleMaterial = ResolvedSpeechBubbleComponent->CreateDynamicMaterialInstance(
			PaintSpeechBubbleMaterialIndex,
			ExistingMaterial);
	}

	if (!ActivePaintSpeechBubbleMaterial)
	{
		return false;
	}

	ActivePaintSpeechBubbleMaterial->SetTextureParameterValue(
		PaintSpeechBubbleRenderTargetParameterName,
		RenderTargetForSpeechBubble);

	ResolvedSpeechBubbleComponent->SetVisibility(true, true);
	ResolvedSpeechBubbleComponent->SetHiddenInGame(false, true);
	ResolvedSpeechBubbleComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ResolvedSpeechBubbleComponent->SetGenerateOverlapEvents(false);
	return true;
}

void UPaintCanvasComponent::HidePaintSpeechBubble()
{
	UPrimitiveComponent* ResolvedSpeechBubbleComponent = ActivePaintSpeechBubbleComponent.Get();
	if (!ResolvedSpeechBubbleComponent)
	{
		ResolvedSpeechBubbleComponent = FindPaintSpeechBubbleComponent();
	}

	if (ResolvedSpeechBubbleComponent)
	{
		ResolvedSpeechBubbleComponent->SetHiddenInGame(true, true);
		ResolvedSpeechBubbleComponent->SetVisibility(false, true);
		ResolvedSpeechBubbleComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	ActivePaintSpeechBubbleComponent = nullptr;
	ActivePaintSpeechBubbleMaterial = nullptr;
	ActivePaintSpeechBubbleRenderTarget = nullptr;
}

UTextureRenderTarget2D* UPaintCanvasComponent::GetActivePaintCanvasRenderTarget() const
{
	return PaintCanvasRenderTarget.Get();
}

UTextureRenderTarget2D* UPaintCanvasComponent::CreatePaintCanvasFaceDecalSnapshot(UTextureRenderTarget2D* SourceRenderTarget)
{
	if (!SourceRenderTarget)
	{
		return nullptr;
	}

	UTextureRenderTarget2D* SnapshotRenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(
		this,
		FMath::Max(SourceRenderTarget->SizeX, 1),
		FMath::Max(SourceRenderTarget->SizeY, 1),
		RTF_RGBA16f,
		FLinearColor::Transparent,
		false,
		false);
	if (!SnapshotRenderTarget)
	{
		return nullptr;
	}

	UCanvas* DrawCanvas = nullptr;
	FVector2D RenderTargetSize = FVector2D::ZeroVector;
	FDrawToRenderTargetContext Context;
	UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, SnapshotRenderTarget, DrawCanvas, RenderTargetSize, Context);

	if (DrawCanvas)
	{
		DrawCanvas->K2_DrawTexture(
			SourceRenderTarget,
			FVector2D::ZeroVector,
			RenderTargetSize,
			FVector2D::ZeroVector,
			FVector2D(1.0, 1.0),
			FLinearColor::White,
			BLEND_Opaque,
			0.0f,
			FVector2D(0.5, 0.5));
	}

	UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Context);
	return SnapshotRenderTarget;
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
	LocalAppliedPaintRevision = 0;
	LocalAppliedPaintLastSequence = 0;
	LocalAppliedPaintChecksum = 0;
	LocalPredictedPaintStrokeCount = 0;
	LocalPredictedPaintChecksum = 0;
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

void UPaintCanvasComponent::SubmitPaintCanvasStrokeForNetwork(UTexture2D* InBrushTexture, double InBrushSize, const FVector2D& DrawLocation)
{
	static_cast<void>(InBrushTexture);
	APdPlayer* PlayerOwner = GetPlayerOwner();
	const double SafeBrushSize = GetClampedReplicatedPaintBrushSize(InBrushSize);
	if (!PlayerOwner
		|| PlayerOwner->GetNetMode() == NM_Standalone
		|| SafeBrushSize <= 0.0
		|| !IsValidReplicatedDrawLocation(DrawLocation))
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
		if (GetClampedReplicatedPaintBrushSize(Request.BrushSize) <= 0.0
			|| !IsValidReplicatedDrawLocation(DrawLocation))
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
			GetClampedReplicatedPaintBrushSize(Request.BrushSize));
		Stroke.DrawLocation = Request.DrawLocation;
		ReplicatedPaintStateHeader.Checksum =
			AccumulatePaintStrokeChecksum(
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
		HandleReplicatedPaintStateChanged();
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

	LocalAppliedPaintRevision = ReplicatedPaintStateHeader.Revision;
	LocalAppliedPaintLastSequence = 0;
	LocalAppliedPaintChecksum = 0;
	LocalPredictedPaintStrokeCount = 0;
	LocalPredictedPaintChecksum = 0;
	if (PlayerOwner->GetNetMode() != NM_DedicatedServer)
	{
		EnsurePaintCanvasRenderResources(true);
	}
}

void UPaintCanvasComponent::HandleReplicatedPaintStateChanged()
{
	SchedulePaintStateReconciliation();
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

uint32 UPaintCanvasComponent::AccumulatePaintStrokeChecksum(
	uint32 CurrentChecksum,
	const FReplicatedPaintCanvasStroke& Stroke)
{
	CurrentChecksum = HashCombineFast(
		CurrentChecksum,
		GetTypeHash(Stroke.Sequence));
	CurrentChecksum = HashCombineFast(
		CurrentChecksum,
		GetTypeHash(Stroke.BrushSize));
	CurrentChecksum = HashCombineFast(
		CurrentChecksum,
		GetTypeHash(Stroke.DrawLocation.X));
	return HashCombineFast(
		CurrentChecksum,
		GetTypeHash(Stroke.DrawLocation.Y));
}

bool UPaintCanvasComponent::TryBuildValidatedAuthoritativeHistory(
	TArray<const FReplicatedPaintCanvasStroke*>& OutSortedStrokes,
	uint32& OutChecksum) const
{
	OutSortedStrokes.Reset();
	OutChecksum = 0;

	if (ReplicatedPaintStateHeader.Revision == 0
		|| ReplicatedPaintStateHeader.LastSequence
			> static_cast<uint32>(FMath::Max(
				MaxReplicatedPaintStrokeHistory,
				1))
		|| ReplicatedPaintStrokes.Items.Num()
			!= static_cast<int32>(
				ReplicatedPaintStateHeader.LastSequence))
	{
		return false;
	}

	OutSortedStrokes.Reserve(ReplicatedPaintStrokes.Items.Num());
	for (const FReplicatedPaintCanvasStroke& Stroke
		: ReplicatedPaintStrokes.Items)
	{
		OutSortedStrokes.Add(&Stroke);
	}

	OutSortedStrokes.Sort(
		[](const FReplicatedPaintCanvasStroke& Left,
			const FReplicatedPaintCanvasStroke& Right)
		{
			return Left.Sequence < Right.Sequence;
		});

	uint32 ExpectedSequence = 1;
	for (const FReplicatedPaintCanvasStroke* Stroke
		: OutSortedStrokes)
	{
		if (!Stroke
			|| Stroke->Sequence != ExpectedSequence
			|| Stroke->BrushSize <= 0.0f
			|| !IsValidReplicatedDrawLocation(
				FVector2D(Stroke->DrawLocation)))
		{
			OutSortedStrokes.Reset();
			OutChecksum = 0;
			return false;
		}

		OutChecksum = AccumulatePaintStrokeChecksum(
			OutChecksum,
			*Stroke);
		++ExpectedSequence;
	}

	if (OutChecksum != ReplicatedPaintStateHeader.Checksum)
	{
		OutSortedStrokes.Reset();
		OutChecksum = 0;
		return false;
	}

	return true;
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

bool UPaintCanvasComponent::IsLocalPaintStateSynchronized(
	const uint32 Revision,
	const uint32 LastSequence,
	const uint32 Checksum) const
{
	return LocalAppliedPaintRevision == Revision
		&& LocalAppliedPaintLastSequence == LastSequence
		&& LocalAppliedPaintChecksum == Checksum;
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
	uint32 ValidatedChecksum = 0;
	if (!TryBuildValidatedAuthoritativeHistory(
		SortedStrokes,
		ValidatedChecksum))
	{
		return;
	}

	if (IsLocalPaintStateSynchronized(
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
		&& LocalPredictedPaintStrokeCount
			== static_cast<int32>(
				ReplicatedPaintStateHeader.LastSequence)
		&& LocalPredictedPaintChecksum == ValidatedChecksum;
	if (!bCanAcceptLocalPrediction)
	{
		bool bAppliedIncrementally = false;
		if (LocalAppliedPaintRevision
				== ReplicatedPaintStateHeader.Revision
			&& LocalAppliedPaintLastSequence
				<= ReplicatedPaintStateHeader.LastSequence
			&& EnsurePaintCanvasRenderResources())
		{
			uint32 RunningChecksum = LocalAppliedPaintChecksum;
			for (const FReplicatedPaintCanvasStroke* Stroke
				: SortedStrokes)
			{
				if (Stroke
					&& Stroke->Sequence
						> LocalAppliedPaintLastSequence)
				{
					DrawBrushToRenderTarget(
						BrushTexture.Get(),
						Stroke->BrushSize,
						FVector2D(Stroke->DrawLocation));
					RunningChecksum = AccumulatePaintStrokeChecksum(
						RunningChecksum,
						*Stroke);
				}
			}

			bAppliedIncrementally =
				RunningChecksum == ValidatedChecksum;
		}

		if (!bAppliedIncrementally)
		{
			RebuildCanvasFromAuthoritativeHistory(SortedStrokes);
		}
	}

	LocalAppliedPaintRevision = ReplicatedPaintStateHeader.Revision;
	LocalAppliedPaintLastSequence =
		ReplicatedPaintStateHeader.LastSequence;
	LocalAppliedPaintChecksum = ValidatedChecksum;
	LocalPredictedPaintStrokeCount = static_cast<int32>(
		ReplicatedPaintStateHeader.LastSequence);
	LocalPredictedPaintChecksum = ValidatedChecksum;
	ProcessPendingPaintPresentation();
}

void UPaintCanvasComponent::ProcessPendingPaintPresentation()
{
	if (bHasPendingPaintExport
		&& IsLocalPaintStateSynchronized(
			PendingPaintExportRevision,
			PendingPaintExportLastSequence,
			PendingPaintExportChecksum))
	{
		bHasPendingPaintExport = false;
		StartLocalPaintCanvasExport();
	}

	if (bHasPendingFaceDecal
		&& IsLocalPaintStateSynchronized(
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

void UPaintCanvasComponent::ClearPaintCanvasFaceDecal()
{
	if (IsValid(ActivePaintCanvasFaceDecalComponent))
	{
		ActivePaintCanvasFaceDecalComponent->DestroyComponent();
	}

	ActivePaintCanvasFaceDecalComponent = nullptr;
	ActivePaintCanvasFaceDecalMaterial = nullptr;
	ActivePaintCanvasFaceDecalSnapshot = nullptr;
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

	if (UPdGameInstance* PdGameInstance = PlayerOwner->GetGameInstance<UPdGameInstance>())
	{
		PdGameInstance->CacheLocalLobbyPaintCanvasStroke(InBrushTexture, InBrushSize, DrawLocation);
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

	if (UPdGameInstance* PdGameInstance = PlayerOwner->GetGameInstance<UPdGameInstance>())
	{
		PdGameInstance->CacheLocalLobbyPaintCanvasFaceDecal(
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
	APdPlayer* PlayerOwner = GetPlayerOwner();
	UTextureRenderTarget2D* PaintRenderTarget = GetActivePaintCanvasRenderTarget();
	USkeletalMeshComponent* CharacterMesh = PlayerOwner ? PlayerOwner->GetMesh() : nullptr;
	if (!PlayerOwner || !FaceDecalMaterial || !PaintRenderTarget || !CharacterMesh)
	{
		return false;
	}

	if (!AttachSocketName.IsNone()
		&& !CharacterMesh->DoesSocketExist(AttachSocketName)
		&& CharacterMesh->GetBoneIndex(AttachSocketName) == INDEX_NONE)
	{
		AttachSocketName = NAME_None;
	}

	ClearPaintCanvasFaceDecal();

	ActivePaintCanvasFaceDecalSnapshot = CreatePaintCanvasFaceDecalSnapshot(PaintRenderTarget);
	if (!ActivePaintCanvasFaceDecalSnapshot)
	{
		return false;
	}

	TextureParameterName = TextureParameterName.IsNone()
		? FName(TEXT("RenderTarget"))
		: TextureParameterName;

	ActivePaintCanvasFaceDecalMaterial = UMaterialInstanceDynamic::Create(FaceDecalMaterial, this);
	if (!ActivePaintCanvasFaceDecalMaterial)
	{
		return false;
	}

	ActivePaintCanvasFaceDecalMaterial->SetTextureParameterValue(TextureParameterName, ActivePaintCanvasFaceDecalSnapshot.Get());

	FaceDecalSize.X = FMath::Max(FaceDecalSize.X, 1.0);
	FaceDecalSize.Y = FMath::Max(FaceDecalSize.Y, 1.0);
	FaceDecalSize.Z = FMath::Max(FaceDecalSize.Z, 1.0);

	const bool bHasAttachSocket = !AttachSocketName.IsNone();
	const FTransform FaceDecalBaseTransform(
		PlayerOwner->GetActorRotation(),
		bHasAttachSocket ? CharacterMesh->GetSocketLocation(AttachSocketName) : PlayerOwner->GetActorLocation(),
		FVector::OneVector);
	FTransform FaceDecalWorldTransform = FaceDecalTransformOffset * FaceDecalBaseTransform;
	FaceDecalWorldTransform.NormalizeRotation();

	FaceDecalWorldTransform.AddToTranslation(FaceDecalWorldTransform.GetUnitAxis(EAxis::X) * (FaceDecalSize.X * 0.5));

	ActivePaintCanvasFaceDecalComponent = UGameplayStatics::SpawnDecalAttached(
		ActivePaintCanvasFaceDecalMaterial.Get(),
		FaceDecalSize,
		CharacterMesh,
		AttachSocketName,
		FaceDecalWorldTransform.GetLocation(),
		FaceDecalWorldTransform.GetRotation().Rotator(),
		EAttachLocation::KeepWorldPosition,
		0.0f);

	if (!IsValid(ActivePaintCanvasFaceDecalComponent))
	{
		ActivePaintCanvasFaceDecalMaterial = nullptr;
		return false;
	}

	ActivePaintCanvasFaceDecalComponent->SetWorldScale3D(FaceDecalWorldTransform.GetScale3D());
	ActivePaintCanvasFaceDecalComponent->SetFadeScreenSize(0.0f);

	return true;
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
		|| !IsValidReplicatedFaceDecalPayload(FaceDecalMaterial, FaceDecalTransformOffset, FaceDecalSize))
	{
		return;
	}

	FlushPendingPaintStrokeBatches(true);

	if (PlayerOwner->HasAuthority())
	{
		if (!TryConsumePaintNetworkEvent(LastPaintFaceDecalServerTime, PaintControlNetworkMinInterval))
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

	if (IsLocalPaintStateSynchronized(
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
	if (!IsValidReplicatedFaceDecalPayload(FaceDecalMaterial, FaceDecalTransformOffset, FaceDecalSize)
		|| !TryConsumePaintNetworkEvent(LastPaintFaceDecalServerTime, PaintControlNetworkMinInterval))
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
		|| !IsValidReplicatedFaceDecalPayload(FaceDecalMaterial, FaceDecalTransformOffset, FaceDecalSize))
	{
		return;
	}

	if (IsLocalPaintStateSynchronized(
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

	UPdGameInstance* PdGameInstance = PlayerOwner->GetGameInstance<UPdGameInstance>();
	if (!PdGameInstance)
	{
		return;
	}

	FLobbyPaintCanvasFaceDecalCache FaceDecalCache;
	if (!PdGameInstance->ConsumeLocalLobbyPaintCanvasFaceDecalCache(FaceDecalCache))
	{
		return;
	}

	ResetLocalSharedPaintCanvas();
	SubmitPaintCanvasResetForNetwork();
	if (!EnsurePaintCanvasRenderResources())
	{
		return;
	}

	const int32 SafeMaxHistory = FMath::Clamp(
		MaxReplicatedPaintStrokeHistory,
		1,
		8192);
	for (int32 StrokeIndex = 0;
		StrokeIndex < FaceDecalCache.Strokes.Num()
			&& LocalPredictedPaintStrokeCount < SafeMaxHistory;
		++StrokeIndex)
	{
		const FLobbyPaintCanvasStrokeCache& Stroke =
			FaceDecalCache.Strokes[StrokeIndex];
		if (Stroke.BrushSize <= 0.0)
		{
			continue;
		}

		const float SafeBrushSize = static_cast<float>(
			GetClampedReplicatedPaintBrushSize(Stroke.BrushSize));
		const FVector2f QuantizedDrawLocation(Stroke.DrawLocation);
		const FVector2D SharedDrawLocation(QuantizedDrawLocation);
		if (SafeBrushSize <= 0.0
			|| !IsValidReplicatedDrawLocation(SharedDrawLocation)
			|| !DrawBrushToRenderTarget(
				BrushTexture.Get(),
				SafeBrushSize,
				SharedDrawLocation))
		{
			continue;
		}

		++LocalPredictedPaintStrokeCount;
		FReplicatedPaintCanvasStroke PredictedStroke;
		PredictedStroke.Sequence = static_cast<uint32>(
			LocalPredictedPaintStrokeCount);
		PredictedStroke.BrushSize = static_cast<float>(SafeBrushSize);
		PredictedStroke.DrawLocation = QuantizedDrawLocation;
		LocalPredictedPaintChecksum = AccumulatePaintStrokeChecksum(
			LocalPredictedPaintChecksum,
			PredictedStroke);
		SubmitPaintCanvasStrokeForNetwork(
			BrushTexture.Get(),
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
