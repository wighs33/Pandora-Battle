#include "Component/Player/PaintCanvasComponent.h"

#include "Camera/PlayerCameraManager.h"
#include "Character/PdPlayer.h"
#include "Components/DecalComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Canvas.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Env/PaintCanvas.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Mode/PdGameInstance.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PaintCanvasComponent)

DEFINE_LOG_CATEGORY_STATIC(PdPaintCanvasComponentLog, Log, All);

namespace
{
	double GetVectorAxisValue(const FVector& Vector, const int32 AxisIndex)
	{
		switch (AxisIndex)
		{
		case 0:
			return Vector.X;
		case 1:
			return Vector.Y;
		default:
			return Vector.Z;
		}
	}

	void FindSurfaceAxes(const FVector& BoundsSize, int32& OutUAxis, int32& OutVAxis)
	{
		int32 ThinAxis = 0;
		double ThinSize = BoundsSize.X;

		if (BoundsSize.Y < ThinSize)
		{
			ThinAxis = 1;
			ThinSize = BoundsSize.Y;
		}

		if (BoundsSize.Z < ThinSize)
		{
			ThinAxis = 2;
		}

		OutUAxis = ThinAxis == 0 ? 1 : 0;
		OutVAxis = ThinAxis == 2 ? 1 : 2;
	}
}

UPaintCanvasComponent::UPaintCanvasComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	static ConstructorHelpers::FClassFinder<AActor> PaintCanvasBlueprintClass(TEXT("/Game/Env/BP_Canvas"));
	if (PaintCanvasBlueprintClass.Succeeded())
	{
		PaintCanvasActorClass = PaintCanvasBlueprintClass.Class;
	}
	else
	{
		PaintCanvasActorClass = APaintCanvas::StaticClass();
	}
}

void UPaintCanvasComponent::BeginPlay()
{
	Super::BeginPlay();
	HidePaintSpeechBubble();
}

void UPaintCanvasComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelPaintCanvasExport();
	ClearPaintCanvasFaceDecal();
	Super::EndPlay(EndPlayReason);
}

void UPaintCanvasComponent::SetSpeechBubbleComponent(UPrimitiveComponent* InSpeechBubbleComponent)
{
	SpeechBubbleComponent = InSpeechBubbleComponent;
}

bool UPaintCanvasComponent::TryPaintAtCursor()
{
	APdPlayer* PlayerOwner = GetPlayerOwner();
	if (!PlayerOwner)
	{
		return false;
	}

	APlayerController* PlayerController = Cast<APlayerController>(PlayerOwner->GetController());
	if (!PlayerController)
	{
		PlayerController = UGameplayStatics::GetPlayerController(PlayerOwner, 0);
	}

	if (!PlayerController)
	{
		return false;
	}

	float MouseX = 0.0f;
	float MouseY = 0.0f;
	if (!PlayerController->GetMousePosition(MouseX, MouseY))
	{
		return false;
	}

	FVector TraceStart = FVector::ZeroVector;
	FVector TraceDirection = FVector::ZeroVector;
	if (!PlayerController->DeprojectScreenPositionToWorld(MouseX, MouseY, TraceStart, TraceDirection)
		|| TraceDirection.IsNearlyZero())
	{
		return false;
	}

	const FVector TraceEnd = TraceStart + TraceDirection * PaintTraceDistance;

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(PlayerOwner);

	FHitResult HitResult;
	const bool bHit = UKismetSystemLibrary::LineTraceSingle(
		PlayerOwner,
		TraceStart,
		TraceEnd,
		PaintTraceChannel.GetValue(),
		true,
		ActorsToIgnore,
		EDrawDebugTrace::None,
		HitResult,
		true);

	return bHit && PaintAtHitResult(HitResult);
}

AActor* UPaintCanvasComponent::ShowPaintCanvasWithCharacterOffset(const FTransform& PaintCanvasTransformOffset)
{
	CancelPaintCanvasExport();

	APdPlayer* PlayerOwner = GetPlayerOwner();
	UWorld* World = GetWorld();
	if (!PlayerOwner || !World || !PaintCanvasActorClass)
	{
		return nullptr;
	}

	FTransform CanvasTransform = PaintCanvasTransformOffset * PlayerOwner->GetActorTransform();
	CanvasTransform.NormalizeRotation();

	if (!IsValid(ActivePaintCanvasActor))
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = PlayerOwner;
		SpawnParameters.Instigator = PlayerOwner;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ActivePaintCanvasActor = World->SpawnActor<AActor>(PaintCanvasActorClass, CanvasTransform, SpawnParameters);
	}
	else
	{
		ActivePaintCanvasActor->SetActorTransform(CanvasTransform, false, nullptr, ETeleportType::TeleportPhysics);
		ActivePaintCanvasActor->SetActorHiddenInGame(false);
		ActivePaintCanvasActor->SetActorEnableCollision(true);
	}

	ConfigurePaintCanvasCollision(ActivePaintCanvasActor.Get());

	if (APaintCanvas* PaintCanvas = Cast<APaintCanvas>(ActivePaintCanvasActor))
	{
		PaintCanvas->NativeInitializeCanvas();
	}

	if (PlayerOwner->IsLocallyControlled())
	{
		if (UPdGameInstance* PdGameInstance = PlayerOwner->GetGameInstance<UPdGameInstance>())
		{
			PdGameInstance->ResetLocalLobbyPaintCanvasCache();
		}
	}
	SubmitPaintCanvasResetForNetwork();
	CenterPaintCanvasVisualOnView(ActivePaintCanvasActor.Get(), CanvasTransform.GetLocation());

	return ActivePaintCanvasActor.Get();
}

void UPaintCanvasComponent::ConfigurePaintCanvasCollision(AActor* PaintCanvasActor) const
{
	if (!IsValid(PaintCanvasActor))
	{
		return;
	}

	if (APaintCanvas* PaintCanvas = Cast<APaintCanvas>(PaintCanvasActor))
	{
		PaintCanvas->ConfigureCollisionForPaintTrace(PaintTraceChannel);
		return;
	}

	const ECollisionChannel PaintCollisionChannel = UEngineTypes::ConvertToCollisionChannel(PaintTraceChannel.GetValue());

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	PaintCanvasActor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);

	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!PrimitiveComponent)
		{
			continue;
		}

		PrimitiveComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		PrimitiveComponent->SetCollisionObjectType(ECC_WorldDynamic);
		PrimitiveComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
		PrimitiveComponent->SetCollisionResponseToChannel(PaintCollisionChannel, ECR_Block);
		PrimitiveComponent->SetGenerateOverlapEvents(false);
	}
}

void UPaintCanvasComponent::HidePaintCanvas()
{
	if (bPaintCanvasExportActive)
	{
		return;
	}

	if (!IsValid(ActivePaintCanvasActor))
	{
		return;
	}

	ActivePaintCanvasActor->SetActorHiddenInGame(true);
	ActivePaintCanvasActor->SetActorEnableCollision(false);
}

bool UPaintCanvasComponent::HasActivePaintCanvas() const
{
	return IsValid(ActivePaintCanvasActor) && !ActivePaintCanvasActor->IsHidden();
}

bool UPaintCanvasComponent::ExportActivePaintCanvasAboveCharacterWithTransformOffset(const FTransform& PaintCanvasExportTransformOffset)
{
	if (!StartLocalPaintCanvasExportAboveCharacter(PaintCanvasExportTransformOffset))
	{
		return false;
	}

	SubmitPaintCanvasExportForNetwork(PaintCanvasExportTransformOffset);
	return true;
}

void UPaintCanvasComponent::CenterPaintCanvasVisualOnView(AActor* PaintCanvasActor, const FVector& DesiredVisualCenter) const
{
	if (!IsValid(PaintCanvasActor))
	{
		return;
	}

	UPrimitiveComponent* PreferredCanvasComponent = nullptr;
	UPrimitiveComponent* FirstVisiblePrimitive = nullptr;
	TArray<UPrimitiveComponent*> PrimitiveComponents;
	PaintCanvasActor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);

	static const FName CanvasComponentName(TEXT("Canvas"));
	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!PrimitiveComponent || !PrimitiveComponent->IsVisible())
		{
			continue;
		}

		if (!FirstVisiblePrimitive && PrimitiveComponent->Bounds.SphereRadius > 0.0f)
		{
			FirstVisiblePrimitive = PrimitiveComponent;
		}

		const FString ComponentName = PrimitiveComponent->GetName();
		if ((PrimitiveComponent->GetFName() == CanvasComponentName || ComponentName.StartsWith(TEXT("Canvas")))
			&& PrimitiveComponent->Bounds.SphereRadius > 0.0f)
		{
			PreferredCanvasComponent = PrimitiveComponent;
			break;
		}
	}

	UPrimitiveComponent* CanvasComponentForBounds = PreferredCanvasComponent ? PreferredCanvasComponent : FirstVisiblePrimitive;
	if (!CanvasComponentForBounds)
	{
		return;
	}

	const FVector VisualCenter = CanvasComponentForBounds->Bounds.Origin;
	const FVector CenterCorrection = DesiredVisualCenter - VisualCenter;
	if (!CenterCorrection.IsNearlyZero())
	{
		PaintCanvasActor->AddActorWorldOffset(CenterCorrection, false, nullptr, ETeleportType::TeleportPhysics);
	}
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

	if (!IsValid(ActivePaintCanvasActor))
	{
		return;
	}

	ActivePaintCanvasActor->SetActorHiddenInGame(true);
	ActivePaintCanvasActor->SetActorEnableCollision(false);
}

bool UPaintCanvasComponent::StartLocalPaintCanvasExportAboveCharacter(const FTransform& PaintCanvasExportTransformOffset)
{
	if (!IsValid(ActivePaintCanvasActor))
	{
		return false;
	}

	CancelPaintCanvasExport();

	if (!ApplyPaintCanvasToSpeechBubble())
	{
		return false;
	}

	bPaintCanvasExportActive = true;

	ActivePaintCanvasActor->SetActorHiddenInGame(true);
	ActivePaintCanvasActor->SetActorEnableCollision(false);

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

bool UPaintCanvasComponent::PaintAtHitResult(const FHitResult& HitResult)
{
	if (!HitResult.GetActor())
	{
		return false;
	}

	FVector2D CollisionUv = FVector2D::ZeroVector;
	if (const APaintCanvas* PaintCanvas = Cast<APaintCanvas>(HitResult.GetActor()))
	{
		if (!PaintCanvas->TryGetDrawLocationFromHitResult(HitResult, CollisionUv))
		{
			return false;
		}
	}
	else if (!TryGetDrawLocationFromHitResult(HitResult, CollisionUv))
	{
		return false;
	}

	UTexture2D* PaintBrushTexture = BrushTexture.Get();
	const double PaintBrushSize = GetClampedReplicatedPaintBrushSize(BrushSize);
	const bool bPainted = DispatchDrawBrush(HitResult.GetActor(), PaintBrushTexture, PaintBrushSize, CollisionUv);
	if (bPainted && HitResult.GetActor() == ActivePaintCanvasActor)
	{
		CacheLocalPaintCanvasStrokeForTravel(PaintBrushTexture, PaintBrushSize, CollisionUv);
		SubmitPaintCanvasStrokeForNetwork(PaintBrushTexture, PaintBrushSize, CollisionUv);
	}

	return bPainted;
}

bool UPaintCanvasComponent::TryGetDrawLocationFromHitResult(const FHitResult& HitResult, FVector2D& OutDrawLocation) const
{
	const UStaticMeshComponent* HitStaticMeshComponent = Cast<UStaticMeshComponent>(HitResult.GetComponent());
	if (!HitStaticMeshComponent || !HitStaticMeshComponent->GetStaticMesh())
	{
		return false;
	}

	const FBox LocalBounds = HitStaticMeshComponent->GetStaticMesh()->GetBoundingBox();
	if (!LocalBounds.IsValid)
	{
		return false;
	}

	const FVector BoundsSize = LocalBounds.GetSize();
	int32 UAxis = 0;
	int32 VAxis = 1;
	FindSurfaceAxes(BoundsSize, UAxis, VAxis);

	const double USize = GetVectorAxisValue(BoundsSize, UAxis);
	const double VSize = GetVectorAxisValue(BoundsSize, VAxis);
	if (FMath::IsNearlyZero(USize) || FMath::IsNearlyZero(VSize))
	{
		return false;
	}

	const FVector WorldHitPoint = HitResult.ImpactPoint.IsNearlyZero() ? HitResult.Location : HitResult.ImpactPoint;
	const FVector LocalHitPoint = HitStaticMeshComponent->GetComponentTransform().InverseTransformPosition(WorldHitPoint);
	const double LocalMinU = GetVectorAxisValue(LocalBounds.Min, UAxis);
	const double LocalMinV = GetVectorAxisValue(LocalBounds.Min, VAxis);

	const double U = (GetVectorAxisValue(LocalHitPoint, UAxis) - LocalMinU) / USize;
	const double V = (GetVectorAxisValue(LocalHitPoint, VAxis) - LocalMinV) / VSize;

	OutDrawLocation = FVector2D(
		FMath::Clamp(U, 0.0, 1.0),
		FMath::Clamp(V, 0.0, 1.0));
	return true;
}

bool UPaintCanvasComponent::DispatchDrawBrush(AActor* HitActor, UTexture2D* InBrushTexture, const double InBrushSize, const FVector2D& DrawLocation) const
{
	const double SafeBrushSize = GetClampedReplicatedPaintBrushSize(InBrushSize);
	if (!IsValid(HitActor) || SafeBrushSize <= 0.0 || !IsValidReplicatedDrawLocation(DrawLocation))
	{
		return false;
	}

	if (APaintCanvas* PaintCanvas = Cast<APaintCanvas>(HitActor))
	{
		PaintCanvas->DrawBrush(InBrushTexture, SafeBrushSize, DrawLocation);
		return true;
	}

	UFunction* DrawBrushFunction = HitActor->FindFunction(TEXT("DrawBrush"));
	if (!DrawBrushFunction)
	{
		return false;
	}

	struct FDrawBrushParams
	{
		UTexture2D* BrushTexture = nullptr;
		double BrushSize = 0.0;
		FVector2D DrawLocation = FVector2D::ZeroVector;
	};

	FDrawBrushParams Params;
	Params.BrushTexture = InBrushTexture;
	Params.BrushSize = SafeBrushSize;
	Params.DrawLocation = DrawLocation;
	HitActor->ProcessEvent(DrawBrushFunction, &Params);
	return true;
}

UTextureRenderTarget2D* UPaintCanvasComponent::GetActivePaintCanvasRenderTarget() const
{
	if (!IsValid(ActivePaintCanvasActor))
	{
		return nullptr;
	}

	if (const APaintCanvas* PaintCanvas = Cast<APaintCanvas>(ActivePaintCanvasActor))
	{
		return PaintCanvas->GetRenderTarget();
	}

	if (const FObjectPropertyBase* RenderTargetProperty = FindFProperty<FObjectPropertyBase>(ActivePaintCanvasActor->GetClass(), TEXT("RenderTarget")))
	{
		return Cast<UTextureRenderTarget2D>(RenderTargetProperty->GetObjectPropertyValue_InContainer(ActivePaintCanvasActor.Get()));
	}

	return nullptr;
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

bool UPaintCanvasComponent::EnsurePaintCanvasActorForSharedDisplay()
{
	if (IsValid(ActivePaintCanvasActor))
	{
		return true;
	}

	APdPlayer* PlayerOwner = GetPlayerOwner();
	UWorld* World = GetWorld();
	if (!PlayerOwner || !World || !PaintCanvasActorClass)
	{
		return false;
	}

	const FTransform CanvasTransform(PlayerOwner->GetActorRotation(), PlayerOwner->GetActorLocation() + PaintCanvasExportOffset);
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = PlayerOwner;
	SpawnParameters.Instigator = PlayerOwner;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ActivePaintCanvasActor = World->SpawnActor<AActor>(PaintCanvasActorClass, CanvasTransform, SpawnParameters);
	if (!IsValid(ActivePaintCanvasActor))
	{
		return false;
	}

	ConfigurePaintCanvasCollision(ActivePaintCanvasActor.Get());
	ActivePaintCanvasActor->SetActorHiddenInGame(true);
	ActivePaintCanvasActor->SetActorEnableCollision(false);
	return true;
}

void UPaintCanvasComponent::ResetLocalSharedPaintCanvas()
{
	CancelPaintCanvasExport();
	if (!EnsurePaintCanvasActorForSharedDisplay())
	{
		return;
	}

	if (APaintCanvas* PaintCanvas = Cast<APaintCanvas>(ActivePaintCanvasActor))
	{
		PaintCanvas->NativeInitializeCanvas();
	}

	ActivePaintCanvasActor->SetActorHiddenInGame(true);
	ActivePaintCanvasActor->SetActorEnableCollision(false);
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
		if (!TryConsumePaintNetworkEvent(LastPaintResetServerTime, PaintControlNetworkMinInterval))
		{
			return;
		}

		MulticastResetSharedPaintCanvas();
		return;
	}

	ServerResetSharedPaintCanvas();
}

void UPaintCanvasComponent::SubmitPaintCanvasStrokeForNetwork(UTexture2D* InBrushTexture, double InBrushSize, const FVector2D& DrawLocation)
{
	APdPlayer* PlayerOwner = GetPlayerOwner();
	const double SafeBrushSize = GetClampedReplicatedPaintBrushSize(InBrushSize);
	if (!PlayerOwner
		|| PlayerOwner->GetNetMode() == NM_Standalone
		|| SafeBrushSize <= 0.0
		|| !IsValidReplicatedDrawLocation(DrawLocation))
	{
		return;
	}

	if (PlayerOwner->HasAuthority())
	{
		if (!TryConsumePaintNetworkEvent(LastPaintStrokeServerTime, PaintStrokeNetworkMinInterval))
		{
			return;
		}

		MulticastSubmitPaintCanvasStroke(InBrushTexture, SafeBrushSize, DrawLocation);
		return;
	}

	ServerSubmitPaintCanvasStroke(InBrushTexture, SafeBrushSize, DrawLocation);
}

void UPaintCanvasComponent::SubmitPaintCanvasExportForNetwork(const FTransform& PaintCanvasExportTransformOffset)
{
	APdPlayer* PlayerOwner = GetPlayerOwner();
	if (!PlayerOwner
		|| PlayerOwner->GetNetMode() == NM_Standalone
		|| !IsValidReplicatedPaintTransform(
			PaintCanvasExportTransformOffset,
			MaxReplicatedPaintExportOffsetDistance,
			MaxReplicatedPaintTransformScale))
	{
		return;
	}

	if (PlayerOwner->HasAuthority())
	{
		if (!TryConsumePaintNetworkEvent(LastPaintExportServerTime, PaintControlNetworkMinInterval))
		{
			return;
		}

		MulticastExportPaintCanvas(PaintCanvasExportTransformOffset);
		return;
	}

	ServerExportPaintCanvas(PaintCanvasExportTransformOffset);
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
			TextureParameterName);
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
	if (!TryConsumePaintNetworkEvent(LastPaintResetServerTime, PaintControlNetworkMinInterval))
	{
		return;
	}

	MulticastResetSharedPaintCanvas();
}

void UPaintCanvasComponent::MulticastResetSharedPaintCanvas_Implementation()
{
	const APdPlayer* PlayerOwner = GetPlayerOwner();
	if (!PlayerOwner || PlayerOwner->GetNetMode() == NM_DedicatedServer || PlayerOwner->IsLocallyControlled())
	{
		return;
	}

	ResetLocalSharedPaintCanvas();
}

void UPaintCanvasComponent::ServerSubmitPaintCanvasStroke_Implementation(UTexture2D* InBrushTexture, double InBrushSize, FVector2D DrawLocation)
{
	const double SafeBrushSize = GetClampedReplicatedPaintBrushSize(InBrushSize);
	if (SafeBrushSize <= 0.0
		|| !IsValidReplicatedDrawLocation(DrawLocation)
		|| !TryConsumePaintNetworkEvent(LastPaintStrokeServerTime, PaintStrokeNetworkMinInterval))
	{
		return;
	}

	MulticastSubmitPaintCanvasStroke(InBrushTexture, SafeBrushSize, DrawLocation);
}

void UPaintCanvasComponent::MulticastSubmitPaintCanvasStroke_Implementation(UTexture2D* InBrushTexture, double InBrushSize, FVector2D DrawLocation)
{
	const APdPlayer* PlayerOwner = GetPlayerOwner();
	const double SafeBrushSize = GetClampedReplicatedPaintBrushSize(InBrushSize);
	if (!PlayerOwner
		|| PlayerOwner->GetNetMode() == NM_DedicatedServer
		|| PlayerOwner->IsLocallyControlled()
		|| SafeBrushSize <= 0.0
		|| !IsValidReplicatedDrawLocation(DrawLocation))
	{
		return;
	}

	if (!EnsurePaintCanvasActorForSharedDisplay())
	{
		return;
	}

	DispatchDrawBrush(ActivePaintCanvasActor.Get(), InBrushTexture, SafeBrushSize, DrawLocation);
}

void UPaintCanvasComponent::ServerExportPaintCanvas_Implementation(FTransform PaintCanvasExportTransformOffset)
{
	if (!IsValidReplicatedPaintTransform(
			PaintCanvasExportTransformOffset,
			MaxReplicatedPaintExportOffsetDistance,
			MaxReplicatedPaintTransformScale)
		|| !TryConsumePaintNetworkEvent(LastPaintExportServerTime, PaintControlNetworkMinInterval))
	{
		return;
	}

	MulticastExportPaintCanvas(PaintCanvasExportTransformOffset);
}

void UPaintCanvasComponent::MulticastExportPaintCanvas_Implementation(FTransform PaintCanvasExportTransformOffset)
{
	const APdPlayer* PlayerOwner = GetPlayerOwner();
	if (!PlayerOwner
		|| PlayerOwner->GetNetMode() == NM_DedicatedServer
		|| PlayerOwner->IsLocallyControlled()
		|| !IsValidReplicatedPaintTransform(
			PaintCanvasExportTransformOffset,
			MaxReplicatedPaintExportOffsetDistance,
			MaxReplicatedPaintTransformScale))
	{
		return;
	}

	if (!EnsurePaintCanvasActorForSharedDisplay())
	{
		return;
	}

	StartLocalPaintCanvasExportAboveCharacter(PaintCanvasExportTransformOffset);
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
		TextureParameterName);
}

void UPaintCanvasComponent::MulticastApplyPaintCanvasFaceDecal_Implementation(
	UMaterialInterface* FaceDecalMaterial,
	FName AttachSocketName,
	FTransform FaceDecalTransformOffset,
	FVector FaceDecalSize,
	FName TextureParameterName)
{
	const APdPlayer* PlayerOwner = GetPlayerOwner();
	if (!PlayerOwner
		|| PlayerOwner->GetNetMode() == NM_DedicatedServer
		|| PlayerOwner->IsLocallyControlled()
		|| !IsValidReplicatedFaceDecalPayload(FaceDecalMaterial, FaceDecalTransformOffset, FaceDecalSize))
	{
		return;
	}

	if (!EnsurePaintCanvasActorForSharedDisplay())
	{
		return;
	}

	ApplyLocalPaintCanvasToFaceDecal(
		FaceDecalMaterial,
		AttachSocketName,
		FaceDecalTransformOffset,
		FaceDecalSize,
		TextureParameterName);
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
	if (!EnsurePaintCanvasActorForSharedDisplay())
	{
		return;
	}

	for (const FLobbyPaintCanvasStrokeCache& Stroke : FaceDecalCache.Strokes)
	{
		if (Stroke.BrushSize <= 0.0)
		{
			continue;
		}

		DispatchDrawBrush(ActivePaintCanvasActor.Get(), Stroke.BrushTexture, Stroke.BrushSize, Stroke.DrawLocation);
		SubmitPaintCanvasStrokeForNetwork(Stroke.BrushTexture, Stroke.BrushSize, Stroke.DrawLocation);
	}

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
