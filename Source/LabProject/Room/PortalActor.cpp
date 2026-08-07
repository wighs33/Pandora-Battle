#include "Room/PortalActor.h"

#include "Camera/CameraTypes.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Common/CollisionChannels.h"
#include "Definition/Room/PortalDefinition.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/MovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Mode/PdPlayerController.h"
#include "NavAreas/NavArea_Obstacle.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PortalActor)

DEFINE_LOG_CATEGORY_STATIC(LogPortalActor, Log, All);

namespace
{
void ConfigurePortalBoxBase(UBoxComponent* BoxComponent, const FVector& Extent)
{
	if (!BoxComponent)
	{
		return;
	}

	BoxComponent->SetBoxExtent(Extent);
	BoxComponent->SetCollisionProfileName(TEXT("Custom"));
	BoxComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BoxComponent->SetGenerateOverlapEvents(true);
	BoxComponent->bDynamicObstacle = true;
	BoxComponent->SetAreaClassOverride(UNavArea_Obstacle::StaticClass());
}
void ConfigurePlayerDetectionBox(UBoxComponent* BoxComponent)
{
	ConfigurePortalBoxBase(BoxComponent, FVector(75.0, 45.0, 100.0));
	if (!BoxComponent)
	{
		return;
	}

	BoxComponent->SetCollisionResponseToAllChannels(ECR_Overlap);
	BoxComponent->SetCollisionResponseToChannel(LabCollisionChannels::OverlapBox(), ECR_Ignore);
}

void ConfigurePortalTeleportBox(UBoxComponent* BoxComponent)
{
	ConfigurePortalBoxBase(BoxComponent, FVector(1500.0, 1500.0, 500.0));
	if (!BoxComponent)
	{
		return;
	}

	BoxComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	BoxComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

template <typename ComponentType>
ComponentType* FindPortalComponentByName(const AActor* Actor, const FName ComponentName)
{
	if (!Actor)
	{
		return nullptr;
	}

	TArray<ComponentType*> Components;
	Actor->GetComponents<ComponentType>(Components);
	for (ComponentType* Component : Components)
	{
		if (Component && Component->GetFName() == ComponentName)
		{
			return Component;
		}
	}

	return nullptr;
}
}

APortalActor::APortalActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	TeleportableActorClass = APawn::StaticClass();

	SceneRootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRootComponent);

	PortalPlaneComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PortalPlane"));
	PortalPlaneComponent->SetupAttachment(SceneRootComponent);
	PortalPlaneComponent->SetCollisionObjectType(ECC_WorldStatic);
	PortalPlaneComponent->SetCollisionProfileName(TEXT("NoCollision"));
	PortalPlaneComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PortalPlaneComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	PortalPlaneComponent->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	PortalPlaneComponent->SetRelativeLocation(FVector(0.0, 0.0, 380.0));
	PortalPlaneComponent->SetRelativeRotation(FRotator(-90.0, 0.0, 0.0));
	PortalPlaneComponent->SetRelativeScale3D(FVector(2.5, 2.5, 2.5));

	ForwardDirectionComponent = CreateDefaultSubobject<UArrowComponent>(TEXT("ForwardDirection"));
	ForwardDirectionComponent->SetupAttachment(PortalPlaneComponent);
	ForwardDirectionComponent->SetRelativeLocation(FVector(-8.0, 0.0, 0.0));
	ForwardDirectionComponent->SetRelativeRotation(FRotator(90.0, 0.0, 0.0));

	FXComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("FX"));
	FXComponent->SetupAttachment(PortalPlaneComponent);
	FXComponent->SetRelativeLocation(FVector(1.422222, 0.0, 0.0));
	FXComponent->SetRelativeRotation(FRotator(90.0, 0.0, 0.0));
	FXComponent->SetRelativeScale3D(FVector(0.2, 0.2, 0.2));

	PlayerDetectionComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("PlayerDetection"));
	PlayerDetectionComponent->SetupAttachment(PortalPlaneComponent);
	PlayerDetectionComponent->SetRelativeLocation(FVector(-8.0, 0.0, 0.0));
	PlayerDetectionComponent->SetRelativeRotation(FRotator(90.0, 0.0, 0.0));
	ConfigurePlayerDetectionBox(PlayerDetectionComponent);

	PortalCameraComponent = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("PortalCamera"));
	PortalCameraComponent->SetupAttachment(PortalPlaneComponent);
	PortalCameraComponent->SetRelativeLocation(FVector(-8.0, 0.0, 0.0));
	PortalCameraComponent->SetRelativeRotation(FRotator(90.0, 0.0, 0.0));
	PortalCameraComponent->bEnableClipPlane = true;
	PortalCameraComponent->CaptureSource = SCS_FinalColorLDR;
	PortalCameraComponent->bCaptureEveryFrame = false;
	PortalCameraComponent->bCaptureOnMovement = false;
	PortalCameraComponent->bAlwaysPersistRenderingState = true;
	PortalCameraComponent->bInheritMainViewCameraPostProcessSettings = true;
	PortalCameraComponent->PostProcessSettings.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::Lumen;
	PortalCameraComponent->PostProcessSettings.ReflectionMethod = EReflectionMethod::Lumen;
	PortalCameraComponent->PostProcessSettings.LumenSurfaceCacheResolution = 1.0f;

	BoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	BoxComponent->SetupAttachment(PortalPlaneComponent);
	BoxComponent->SetRelativeLocation(FVector(-8.888889, 0.0, 0.0));
	BoxComponent->SetRelativeRotation(FRotator(90.0, 0.0, 0.0));
	BoxComponent->SetRelativeScale3D(FVector(1.111111, 1.111111, 1.111111));
	ConfigurePortalTeleportBox(BoxComponent);
}

void APortalActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyPortalDefinition();
}

#if WITH_EDITOR
EDataValidationResult APortalActor::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (Result == EDataValidationResult::NotValidated)
	{
		Result = EDataValidationResult::Valid;
	}

	// The native base CDO intentionally has no project content dependency. Every
	// concrete portal Blueprint or placed instance must provide the soft definition.
	if (HasAnyFlags(RF_ClassDefaultObject) && GetClass() == StaticClass())
	{
		return Result;
	}

	if (PortalDefinition.IsNull())
	{
		Context.AddError(NSLOCTEXT(
			"PortalActor",
			"MissingPortalDefinition",
			"PortalDefinition must be assigned so portal assets and capture settings are included in cook validation."));
		return EDataValidationResult::Invalid;
	}

	if (!PortalDefinition.LoadSynchronous())
	{
		Context.AddError(FText::Format(
			NSLOCTEXT(
				"PortalActor",
				"InvalidPortalDefinition",
				"PortalDefinition '{0}' could not be loaded."),
			FText::FromString(PortalDefinition.ToSoftObjectPath().ToString())));
		return EDataValidationResult::Invalid;
	}

	return Result;
}
#endif

void APortalActor::BeginPlay()
{
	Super::BeginPlay();
	ResolvePortalComponents();

	if (UBoxComponent* PortalBox = GetBoxComponent())
	{
		PortalBox->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::HandlePortalBeginOverlap);
		PortalBox->OnComponentEndOverlap.AddUniqueDynamic(this, &ThisClass::HandlePortalEndOverlap);
		SeedTeleportOverlapCache(PortalBox, PortalOverlappingTeleportActors);
	}

	if (UBoxComponent* DetectionBox = GetPlayerDetectionComponent())
	{
		DetectionBox->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::HandleDetectionBeginOverlap);
		DetectionBox->OnComponentEndOverlap.AddUniqueDynamic(this, &ThisClass::HandleDetectionEndOverlap);
		SeedTeleportOverlapCache(DetectionBox, DetectedTeleportActors);
	}

	const bool bPortalDefinitionReady = LoadedPortalDefinition || ApplyPortalDefinition();
	if (bPortalDefinitionReady)
	{
		NativeTryInitPortalMaterial();

		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				InitMaterialTimerHandle,
				this,
				&ThisClass::NativeTryInitPortalMaterial,
				FMath::Max(InitRetryInterval, 0.01f),
				true);
		}
	}
	else
	{
		UE_LOG(LogPortalActor, Error, TEXT("PortalDefinition or one of its required assets could not be loaded for %s: %s"),
			*GetPathName(), *PortalDefinition.ToSoftObjectPath().ToString());
	}

	SetTickEnabledFromOverlaps();
}

void APortalActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InitMaterialTimerHandle);
	}

	PortalOverlappingTeleportActors.Reset();
	DetectedTeleportActors.Reset();
	TraversalStates.Reset();

	if (APortalActor* LinkedPortalActor = GetLinkedPortalActor())
	{
		if (USceneCaptureComponent2D* LinkedCapture = LinkedPortalActor->GetPortalCameraComponent();
			LinkedCapture && LinkedCapture->TextureTarget == PortalRT)
		{
			LinkedCapture->TextureTarget = nullptr;
		}
	}

	PortalRT = nullptr;
	PortalMat = nullptr;
	PortalMaterialParent = nullptr;
	LoadedPortalDefinition = nullptr;

	Super::EndPlay(EndPlayReason);
}

void APortalActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	NativeUpdateSceneCapture();
	NativeTryTeleportOverlappingActor();
}

void APortalActor::NativeTryInitPortalMaterial()
{
	UStaticMeshComponent* ResolvedPortalPlane = GetPortalPlaneComponent();
	if (!ResolvedPortalPlane)
	{
		return;
	}

	if (!PortalMat)
	{
		UMaterialInterface* MaterialParent = PortalMaterialParent;
		if (!MaterialParent)
		{
			MaterialParent = ResolvedPortalPlane->GetMaterial(0);
		}

		if (MaterialParent)
		{
			PortalMat = UMaterialInstanceDynamic::Create(MaterialParent, this);
			ResolvedPortalPlane->SetMaterial(0, PortalMat);
		}
	}

	EnsureRenderTargetSize();

	if (PortalMat && PortalRT && TextureParameterName != NAME_None)
	{
		PortalMat->SetTextureParameterValue(TextureParameterName, PortalRT);
	}

	ConfigureLinkedCaptureComponent();
	UpdatePortalVisualParameters();

	const APortalActor* LinkedPortalActor = GetLinkedPortalActor();
	const USceneCaptureComponent2D* LinkedCapture = LinkedPortalActor ? LinkedPortalActor->GetPortalCameraComponent() : nullptr;
	const bool bReady = PortalMat && PortalRT && LinkedCapture && LinkedCapture->TextureTarget == PortalRT;
	if (bReady)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(InitMaterialTimerHandle);
		}
	}
}

void APortalActor::NativeUpdateSceneCapture()
{
	if (!IsCaptureRateLimitElapsed())
	{
		return;
	}

	APortalActor* LinkedPortalActor = GetLinkedPortalActor();
	USceneCaptureComponent2D* LinkedCapture = LinkedPortalActor ? LinkedPortalActor->GetPortalCameraComponent() : nullptr;
	if (!LinkedPortalActor || !LinkedCapture)
	{
		return;
	}

	APlayerCameraManager* CameraManager = GetCachedPlayerCameraManager();
	if (!CameraManager)
	{
		return;
	}

	EnsureRenderTargetSize();
	ConfigureLinkedCaptureComponent();

	LinkedCapture->SetWorldLocationAndRotation(
		TransformLocationToLinkedPortal(CameraManager->GetCameraLocation()),
		TransformRotationToLinkedPortal(CameraManager->GetCameraRotation()),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);

	if (bCopyPlayerCameraFov)
	{
		LinkedCapture->FOVAngle = FMath::Clamp(CameraManager->GetFOVAngle(), 5.0f, 170.0f);
	}

	if (bCopyPlayerCameraPostProcess)
	{
		const FMinimalViewInfo& CameraView = CameraManager->GetCameraCacheView();
		LinkedCapture->PostProcessSettings = CameraView.PostProcessSettings;
		LinkedCapture->PostProcessBlendWeight = CameraView.PostProcessBlendWeight;
		LinkedCapture->bInheritMainViewCameraPostProcessSettings = true;
	}

	if (PortalRT)
	{
		LinkedCapture->CaptureScene();
	}
}

bool APortalActor::NativeTryTeleportOverlappingActor()
{
	if (!GetLinkedPortalActor())
	{
		return false;
	}

	if (bTeleportOnlyOnAuthority && !HasAuthority())
	{
		return false;
	}

	TArray<TWeakObjectPtr<AActor>> OverlappingActors;
	ResolveOverlappingTeleportActors(OverlappingActors);
	if (OverlappingActors.IsEmpty())
	{
		return false;
	}

	bool bTeleportedAnyActor = false;
	const UPrimitiveComponent* DetectionComponent = GetPlayerDetectionComponent();
	for (const TWeakObjectPtr<AActor>& WeakActor : OverlappingActors)
	{
		AActor* Actor = WeakActor.Get();
		if (!IsTeleportCandidate(Actor)
			|| !DetectionComponent
			|| !DetectionComponent->IsOverlappingActor(Actor)
			|| !IsPointCrossingPortal(Actor, Actor->GetActorLocation()))
		{
			continue;
		}

		TeleportActorThroughPortal(Actor);
		bTeleportedAnyActor = true;
	}

	return bTeleportedAnyActor;
}

FVector APortalActor::GetPortalForward() const
{
	if (const UArrowComponent* DirectionComponent = GetForwardDirectionComponent())
	{
		return DirectionComponent->GetForwardVector();
	}

	return GetActorForwardVector();
}

FVector APortalActor::GetPortalPlaneLocation() const
{
	if (const UStaticMeshComponent* ResolvedPortalPlane = GetPortalPlaneComponent())
	{
		return ResolvedPortalPlane->GetComponentLocation();
	}

	return GetActorLocation();
}

void APortalActor::HandlePortalBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (TrackTeleportOverlap(PortalOverlappingTeleportActors, OtherActor))
	{
		SetTickEnabledFromOverlaps();
	}
}

void APortalActor::HandlePortalEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	UntrackTeleportOverlap(PortalOverlappingTeleportActors, OtherActor, OverlappedComponent);
	RemoveTraversalStateIfNoLongerOverlapping(OtherActor);

	SetTickEnabledFromOverlaps();
}

void APortalActor::HandleDetectionBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (TrackTeleportOverlap(DetectedTeleportActors, OtherActor))
	{
		SetTickEnabledFromOverlaps();
	}
}

void APortalActor::HandleDetectionEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	UntrackTeleportOverlap(DetectedTeleportActors, OtherActor, OverlappedComponent);
	RemoveTraversalStateIfNoLongerOverlapping(OtherActor);

	SetTickEnabledFromOverlaps();
}

bool APortalActor::EnsureRenderTargetSize()
{
	if (!LoadedPortalDefinition)
	{
		return false;
	}

	const FIntPoint DesiredSize = GetDesiredRenderTargetSize();
	if (DesiredSize.X <= 0 || DesiredSize.Y <= 0)
	{
		return false;
	}

	const ETextureRenderTargetFormat DesiredFormat = LoadedPortalDefinition->RenderTargetFormat.GetValue();
	const bool bNeedsNewTarget = !PortalRT;
	const bool bNeedsResize = PortalRT
		&& (PortalRT->SizeX != DesiredSize.X
			|| PortalRT->SizeY != DesiredSize.Y
			|| PortalRT->RenderTargetFormat != DesiredFormat);

	if (!bNeedsNewTarget && !bNeedsResize)
	{
		return true;
	}

	if (!PortalRT)
	{
		PortalRT = NewObject<UTextureRenderTarget2D>(this, TEXT("PortalRT"));
	}

	PortalRT->RenderTargetFormat = DesiredFormat;
	PortalRT->ClearColor = FLinearColor::Black;
	PortalRT->InitAutoFormat(DesiredSize.X, DesiredSize.Y);
	PortalRT->UpdateResourceImmediate(true);

	if (PortalMat && TextureParameterName != NAME_None)
	{
		PortalMat->SetTextureParameterValue(TextureParameterName, PortalRT);
	}

	if (APortalActor* LinkedPortalActor = GetLinkedPortalActor())
	{
		if (USceneCaptureComponent2D* LinkedCapture = LinkedPortalActor->GetPortalCameraComponent())
		{
			LinkedCapture->TextureTarget = PortalRT;
		}
	}

	return true;
}

FIntPoint APortalActor::GetDesiredRenderTargetSize() const
{
	FIntPoint ViewportSize = LoadedPortalDefinition->FallbackViewportSize;

	if (GEngine && GEngine->GameViewport)
	{
		FVector2D RuntimeViewportSize = FVector2D::ZeroVector;
		GEngine->GameViewport->GetViewportSize(RuntimeViewportSize);

		const int32 ViewportX = FMath::TruncToInt(RuntimeViewportSize.X);
		const int32 ViewportY = FMath::TruncToInt(RuntimeViewportSize.Y);
		if (ViewportX > 0 && ViewportY > 0)
		{
			ViewportSize = FIntPoint(ViewportX, ViewportY);
		}
	}

	const float ResolutionScale = FMath::Clamp(LoadedPortalDefinition->ResolutionScale, 0.1f, 1.0f);
	const int32 MaxDimension = FMath::Clamp(LoadedPortalDefinition->MaxRenderTargetDimension, 256, 4096);
	const float MaxDimensionScale = static_cast<float>(MaxDimension)
		/ static_cast<float>(FMath::Max(ViewportSize.X, ViewportSize.Y));
	const float FinalScale = FMath::Min(ResolutionScale, MaxDimensionScale);

	const int32 Width = FMath::Clamp(
		FMath::RoundToInt(static_cast<float>(ViewportSize.X) * FinalScale),
		16,
		MaxDimension);
	const int32 Height = FMath::Clamp(
		FMath::RoundToInt(static_cast<float>(ViewportSize.Y) * FinalScale),
		16,
		MaxDimension);
	return FIntPoint(Width, Height);
}

bool APortalActor::ApplyPortalDefinition()
{
	UPortalDefinition* ResolvedDefinition = PortalDefinition.LoadSynchronous();
	if (!ResolvedDefinition)
	{
		LoadedPortalDefinition = nullptr;
		PortalMaterialParent = nullptr;
		return false;
	}

	UStaticMeshComponent* ResolvedPortalPlane = GetPortalPlaneComponent();
	UNiagaraComponent* ResolvedFXComponent = GetFXComponent();
	UStaticMesh* PortalMesh = ResolvedDefinition->PortalPlaneMesh.LoadSynchronous();
	UMaterialInterface* PortalMaterial = ResolvedDefinition->PortalMaterial.LoadSynchronous();
	UNiagaraSystem* PortalEffect = ResolvedDefinition->PortalEffect.LoadSynchronous();
	if (!ResolvedPortalPlane || !ResolvedFXComponent || !PortalMesh || !PortalMaterial || !PortalEffect)
	{
		LoadedPortalDefinition = nullptr;
		PortalMaterialParent = nullptr;
		return false;
	}

	LoadedPortalDefinition = ResolvedDefinition;
	PortalMaterialParent = PortalMaterial;
	if (ResolvedPortalPlane->GetStaticMesh() != PortalMesh)
	{
		ResolvedPortalPlane->SetStaticMesh(PortalMesh);
	}
	if (ResolvedFXComponent->GetAsset() != PortalEffect)
	{
		ResolvedFXComponent->SetAsset(PortalEffect);
	}
	return true;
}

bool APortalActor::IsCaptureRateLimitElapsed()
{
	if (!LoadedPortalDefinition)
	{
		return false;
	}

	const float MaxCaptureFrameRate = FMath::Clamp(LoadedPortalDefinition->MaxCaptureFrameRate, 0.0f, 120.0f);
	if (MaxCaptureFrameRate <= 0.0f)
	{
		return true;
	}

	const double CurrentTime = GetWorld()->GetRealTimeSeconds();
	const double MinimumInterval = 1.0 / static_cast<double>(MaxCaptureFrameRate);
	if (CurrentTime - LastSceneCaptureTime < MinimumInterval)
	{
		return false;
	}

	LastSceneCaptureTime = CurrentTime;
	return true;
}

void APortalActor::ConfigureLinkedCaptureComponent() const
{
	APortalActor* LinkedPortalActor = GetLinkedPortalActor();
	USceneCaptureComponent2D* CaptureComponent = LinkedPortalActor ? LinkedPortalActor->GetPortalCameraComponent() : nullptr;
	if (!LinkedPortalActor || !CaptureComponent)
	{
		return;
	}

	CaptureComponent->TextureTarget = PortalRT;
	CaptureComponent->bCaptureEveryFrame = false;
	CaptureComponent->bCaptureOnMovement = false;
	CaptureComponent->bAlwaysPersistRenderingState = true;
	CaptureComponent->bInheritMainViewCameraPostProcessSettings = bCopyPlayerCameraPostProcess;
	CaptureComponent->bUseCustomProjectionMatrix = false;
	CaptureComponent->CaptureSource = SCS_FinalColorLDR;

	CaptureComponent->bEnableClipPlane = bEnablePortalClipPlane;
	if (bEnablePortalClipPlane)
	{
		const FVector LinkedPortalForward = LinkedPortalActor->GetPortalForward();
		CaptureComponent->ClipPlaneBase = LinkedPortalActor->GetPortalPlaneLocation() + LinkedPortalForward * ClipPlaneOffset;
		CaptureComponent->ClipPlaneNormal = LinkedPortalForward;
	}
}

void APortalActor::UpdatePortalVisualParameters() const
{
	const FVector PortalForward = GetPortalForward();

	if (PortalMat && OffsetDistanceParameterName != NAME_None)
	{
		const FVector OffsetDistance = PortalForward * GetBlueprintOffsetAmount();
		PortalMat->SetVectorParameterValue(OffsetDistanceParameterName, FLinearColor(OffsetDistance.X, OffsetDistance.Y, OffsetDistance.Z, 1.0f));
	}

	if (UNiagaraComponent* ResolvedFXComponent = GetFXComponent(); ResolvedFXComponent && VortexVectorParameterName != NAME_None)
	{
		ResolvedFXComponent->SetVariableVec3(VortexVectorParameterName, PortalForward);
	}
}

void APortalActor::SetTickEnabledFromOverlaps()
{
	if (!bUpdateOnlyWhenOverlapping)
	{
		SetActorTickEnabled(true);
		return;
	}

	SetActorTickEnabled(HasTrackedTeleportOverlap(
		PortalOverlappingTeleportActors,
		GetBoxComponent()));
}

void APortalActor::SeedTeleportOverlapCache(UPrimitiveComponent* OverlapComponent, TArray<TWeakObjectPtr<AActor>>& OutActors)
{
	OutActors.Reset();
	if (!OverlapComponent)
	{
		return;
	}

	TArray<AActor*> OverlappingActors;
	OverlapComponent->GetOverlappingActors(OverlappingActors);
	for (AActor* Actor : OverlappingActors)
	{
		TrackTeleportOverlap(OutActors, Actor);
	}
}

bool APortalActor::TrackTeleportOverlap(TArray<TWeakObjectPtr<AActor>>& OverlappingActors, AActor* Actor) const
{
	if (!IsTeleportCandidate(Actor))
	{
		return false;
	}

	for (const TWeakObjectPtr<AActor>& ExistingActor : OverlappingActors)
	{
		if (ExistingActor.Get() == Actor)
		{
			return true;
		}
	}

	OverlappingActors.Add(Actor);
	return true;
}

void APortalActor::UntrackTeleportOverlap(
	TArray<TWeakObjectPtr<AActor>>& OverlappingActors,
	AActor* Actor,
	const UPrimitiveComponent* OverlapComponent) const
{
	if (!Actor)
	{
		OverlappingActors.RemoveAllSwap(
			[](const TWeakObjectPtr<AActor>& ExistingActor)
			{
				return !ExistingActor.IsValid();
			});
		return;
	}
	if (OverlapComponent && OverlapComponent->IsOverlappingActor(Actor))
	{
		return;
	}

	OverlappingActors.RemoveAllSwap(
		[Actor](const TWeakObjectPtr<AActor>& ExistingActor)
		{
			return !ExistingActor.IsValid() || ExistingActor.Get() == Actor;
		});
}

bool APortalActor::HasTrackedTeleportOverlap(
	TArray<TWeakObjectPtr<AActor>>& OverlappingActors,
	const UPrimitiveComponent* OverlapComponent) const
{
	for (int32 ActorIndex = OverlappingActors.Num() - 1; ActorIndex >= 0; --ActorIndex)
	{
		AActor* Actor = OverlappingActors[ActorIndex].Get();
		if (!IsTeleportCandidate(Actor)
			|| !OverlapComponent
			|| !OverlapComponent->IsOverlappingActor(Actor))
		{
			OverlappingActors.RemoveAtSwap(ActorIndex);
			continue;
		}

		return true;
	}

	return false;
}

void APortalActor::RemoveTraversalStateIfNoLongerOverlapping(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	const UPrimitiveComponent* PortalComponent = GetBoxComponent();
	const UPrimitiveComponent* DetectionComponent = GetPlayerDetectionComponent();
	const bool bStillOverlappingPortal = PortalComponent
		&& PortalComponent->IsOverlappingActor(Actor);
	const bool bStillDetected = DetectionComponent
		&& DetectionComponent->IsOverlappingActor(Actor);
	if (!bStillOverlappingPortal && !bStillDetected)
	{
		TraversalStates.Remove(TObjectKey<AActor>(Actor));
	}
}

bool APortalActor::IsTeleportCandidate(const AActor* Actor) const
{
	if (!IsValid(Actor) || Actor == this || Actor == GetLinkedPortalActor())
	{
		return false;
	}

	return !TeleportableActorClass || Actor->IsA(TeleportableActorClass);
}

void APortalActor::ResolveOverlappingTeleportActors(
	TArray<TWeakObjectPtr<AActor>>& OutActors)
{
	OutActors.Reset();
	const UPrimitiveComponent* DetectionComponent = GetPlayerDetectionComponent();
	for (int32 ActorIndex = DetectedTeleportActors.Num() - 1; ActorIndex >= 0; --ActorIndex)
	{
		AActor* Actor = DetectedTeleportActors[ActorIndex].Get();
		if (IsTeleportCandidate(Actor)
			&& DetectionComponent
			&& DetectionComponent->IsOverlappingActor(Actor))
		{
			OutActors.Add(Actor);
			continue;
		}

		DetectedTeleportActors.RemoveAtSwap(ActorIndex);
		RemoveTraversalStateIfNoLongerOverlapping(Actor);
	}
}

APortalActor* APortalActor::GetLinkedPortalActor() const
{
	return LinkedPortal.Get();
}

APlayerCameraManager* APortalActor::GetCachedPlayerCameraManager() const
{
	if (!CachedPlayerCameraManager.IsValid())
	{
		CachedPlayerCameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0);
	}

	return CachedPlayerCameraManager.Get();
}

float APortalActor::GetBlueprintOffsetAmount() const
{
	const FProperty* OffsetProperty = FindFProperty<FProperty>(GetClass(), TEXT("OffsetAmount"));
	if (const FFloatProperty* FloatProperty = CastField<FFloatProperty>(OffsetProperty))
	{
		return FloatProperty->GetPropertyValue_InContainer(this);
	}

	if (const FDoubleProperty* DoubleProperty = CastField<FDoubleProperty>(OffsetProperty))
	{
		return static_cast<float>(DoubleProperty->GetPropertyValue_InContainer(this));
	}

	return PortalEffectOffsetAmount;
}

void APortalActor::ResolvePortalComponents() const
{
	if (!CachedPortalPlane.IsValid())
	{
		CachedPortalPlane = PortalPlaneComponent.Get();
		if (!CachedPortalPlane.IsValid())
		{
			CachedPortalPlane = FindPortalComponentByName<UStaticMeshComponent>(this, TEXT("PortalPlane"));
		}
	}
	if (!CachedBox.IsValid())
	{
		CachedBox = BoxComponent.Get();
		if (!CachedBox.IsValid())
		{
			CachedBox = FindPortalComponentByName<UBoxComponent>(this, TEXT("Box"));
		}
	}
	if (!CachedPlayerDetection.IsValid())
	{
		CachedPlayerDetection = PlayerDetectionComponent.Get();
		if (!CachedPlayerDetection.IsValid())
		{
			CachedPlayerDetection = FindPortalComponentByName<UBoxComponent>(this, TEXT("PlayerDetection"));
		}
	}
	if (!CachedForwardDirection.IsValid())
	{
		CachedForwardDirection = ForwardDirectionComponent.Get();
		if (!CachedForwardDirection.IsValid())
		{
			CachedForwardDirection = FindPortalComponentByName<UArrowComponent>(this, TEXT("ForwardDirection"));
		}
	}
	if (!CachedPortalCamera.IsValid())
	{
		CachedPortalCamera = PortalCameraComponent.Get();
		if (!CachedPortalCamera.IsValid())
		{
			CachedPortalCamera = FindPortalComponentByName<USceneCaptureComponent2D>(this, TEXT("PortalCamera"));
		}
	}
	if (!CachedFX.IsValid())
	{
		CachedFX = FXComponent.Get();
		if (!CachedFX.IsValid())
		{
			CachedFX = FindPortalComponentByName<UNiagaraComponent>(this, TEXT("FX"));
		}
	}
}

UStaticMeshComponent* APortalActor::GetPortalPlaneComponent() const
{
	ResolvePortalComponents();
	return CachedPortalPlane.Get();
}

UBoxComponent* APortalActor::GetBoxComponent() const
{
	ResolvePortalComponents();
	return CachedBox.Get();
}

UBoxComponent* APortalActor::GetPlayerDetectionComponent() const
{
	ResolvePortalComponents();
	return CachedPlayerDetection.Get();
}

UArrowComponent* APortalActor::GetForwardDirectionComponent() const
{
	ResolvePortalComponents();
	return CachedForwardDirection.Get();
}

USceneCaptureComponent2D* APortalActor::GetPortalCameraComponent() const
{
	ResolvePortalComponents();
	return CachedPortalCamera.Get();
}

UNiagaraComponent* APortalActor::GetFXComponent() const
{
	ResolvePortalComponents();
	return CachedFX.Get();
}

bool APortalActor::IsPointCrossingPortal(AActor* Actor, const FVector& Point)
{
	if (!Actor)
	{
		return false;
	}

	const FVector PortalLocation = GetPortalPlaneLocation();
	const FVector PortalNormal = GetPortalForward().GetSafeNormal();
	if (PortalNormal.IsNearlyZero())
	{
		return false;
	}

	FPortalTraversalState& State = TraversalStates.FindOrAdd(TObjectKey<AActor>(Actor));
	const float CurrentDistance = FVector::DotProduct(Point - PortalLocation, PortalNormal);
	const bool bIsInFront = CurrentDistance >= 0.0f;

	if (!State.bInitialized)
	{
		State.LastPosition = Point;
		State.bLastInFront = bIsInFront;
		State.bInitialized = true;
		return false;
	}

	const double CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	const bool bCoolingDown = CurrentTime - State.LastTeleportTime < TeleportCooldown;
	const float LastDistance = FVector::DotProduct(State.LastPosition - PortalLocation, PortalNormal);

	FVector PlaneIntersection = FVector::ZeroVector;
	const bool bSegmentHitsPlane = FMath::SegmentPlaneIntersection(State.LastPosition, Point, FPlane(PortalLocation, PortalNormal), PlaneIntersection);
	const bool bCrossedPlane = State.bLastInFront && !bIsInFront && bSegmentHitsPlane && LastDistance > CrossingDistanceTolerance && CurrentDistance <= CrossingDistanceTolerance;

	State.LastPosition = Point;
	State.bLastInFront = bIsInFront;

	return bCrossedPlane && !bCoolingDown;
}

void APortalActor::TeleportActorThroughPortal(AActor* Actor)
{
	APortalActor* LinkedPortalActor = GetLinkedPortalActor();
	if (!Actor || !LinkedPortalActor)
	{
		return;
	}

	const FVector TargetLocation = TransformLocationToLinkedPortal(Actor->GetActorLocation()) + LinkedPortalActor->GetPortalForward() * ExitOffset;
	const FRotator TargetRotation = TransformRotationToLinkedPortal(Actor->GetActorRotation());

	FVector TargetVelocity = FVector::ZeroVector;
	UMovementComponent* MovementComponent = nullptr;
	if (APawn* Pawn = Cast<APawn>(Actor))
	{
		MovementComponent = Pawn->GetMovementComponent();
	}
	if (!MovementComponent)
	{
		MovementComponent = Actor->FindComponentByClass<UMovementComponent>();
	}
	if (MovementComponent)
	{
		TargetVelocity = TransformVelocityToLinkedPortal(MovementComponent->Velocity);
	}

	if (ACharacter* Character = Cast<ACharacter>(Actor))
	{
		Character->TeleportTo(TargetLocation, TargetRotation, false, true);
	}
	else
	{
		Actor->SetActorLocationAndRotation(TargetLocation, TargetRotation, false, nullptr, ETeleportType::TeleportPhysics);
	}

	if (APawn* Pawn = Cast<APawn>(Actor))
	{
		if (AController* Controller = Pawn->GetController())
		{
			const FRotator TargetControlRotation =
				TransformRotationToLinkedPortal(Controller->GetControlRotation());
			Controller->SetControlRotation(TargetControlRotation);

			if (APdPlayerController* PlayerController = Cast<APdPlayerController>(Controller);
				HasAuthority() && PlayerController && !PlayerController->IsLocalController())
			{
				PlayerController->Client_ApplyPortalTeleport(
					TargetLocation,
					TargetRotation,
					TargetVelocity,
					TargetControlRotation);
			}
		}
	}

	if (MovementComponent)
	{
		MovementComponent->Velocity = TargetVelocity;
	}
	Actor->ForceNetUpdate();

	PrimeTraversalState(Actor);
	LinkedPortalActor->PrimeTraversalState(Actor);
}

void APortalActor::PrimeTraversalState(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	FPortalTraversalState& State = TraversalStates.FindOrAdd(TObjectKey<AActor>(Actor));
	State.LastPosition = Actor->GetActorLocation();
	State.bLastInFront = FVector::DotProduct(State.LastPosition - GetPortalPlaneLocation(), GetPortalForward()) >= 0.0f;
	State.bInitialized = true;
	State.LastTeleportTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

FVector APortalActor::TransformLocationToLinkedPortal(const FVector& WorldLocation) const
{
	APortalActor* LinkedPortalActor = GetLinkedPortalActor();
	if (!LinkedPortalActor)
	{
		return WorldLocation;
	}

	const FTransform SourceTransform = GetPortalReferenceTransform();
	const FTransform TargetTransform = LinkedPortalActor->GetPortalReferenceTransform();

	FVector LocalLocation = SourceTransform.InverseTransformPositionNoScale(WorldLocation);
	LocalLocation.X *= -1.0f;
	LocalLocation.Y *= -1.0f;
	return TargetTransform.TransformPositionNoScale(LocalLocation);
}

FVector APortalActor::TransformDirectionToLinkedPortal(const FVector& WorldDirection) const
{
	APortalActor* LinkedPortalActor = GetLinkedPortalActor();
	if (!LinkedPortalActor)
	{
		return WorldDirection;
	}

	const FTransform SourceTransform = GetPortalReferenceTransform();
	const FTransform TargetTransform = LinkedPortalActor->GetPortalReferenceTransform();

	FVector LocalDirection = SourceTransform.InverseTransformVectorNoScale(WorldDirection);
	LocalDirection.X *= -1.0f;
	LocalDirection.Y *= -1.0f;
	return TargetTransform.TransformVectorNoScale(LocalDirection).GetSafeNormal();
}

FRotator APortalActor::TransformRotationToLinkedPortal(const FRotator& WorldRotation) const
{
	const FRotationMatrix RotationMatrix(WorldRotation);
	const FVector Forward = TransformDirectionToLinkedPortal(RotationMatrix.GetScaledAxis(EAxis::X));
	const FVector Right = TransformDirectionToLinkedPortal(RotationMatrix.GetScaledAxis(EAxis::Y));
	const FVector Up = TransformDirectionToLinkedPortal(RotationMatrix.GetScaledAxis(EAxis::Z));

	return UKismetMathLibrary::MakeRotationFromAxes(Forward, Right, Up);
}

FVector APortalActor::TransformVelocityToLinkedPortal(const FVector& WorldVelocity) const
{
	const double Speed = WorldVelocity.Size();
	if (Speed <= UE_KINDA_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}

	return TransformDirectionToLinkedPortal(WorldVelocity / Speed) * Speed;
}

FTransform APortalActor::GetPortalReferenceTransform() const
{
	const FQuat PortalRotation = GetForwardDirectionComponent()
		? GetForwardDirectionComponent()->GetComponentQuat()
		: GetActorQuat();

	return FTransform(PortalRotation, GetPortalPlaneLocation(), FVector::OneVector);
}
