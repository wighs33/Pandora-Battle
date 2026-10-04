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
#include "Materials/MaterialInstanceDynamic.h"
#include "Mode/PdPlayerController.h"
#include "Room/PortalSpace.h"
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

	const auto IsCandidate = [this](const AActor* Actor) { return IsTeleportCandidate(Actor); };
	BoxComponent->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::HandlePortalBeginOverlap);
	BoxComponent->OnComponentEndOverlap.AddUniqueDynamic(this, &ThisClass::HandlePortalEndOverlap);
	PortalBoxOverlaps.Seed(BoxComponent, IsCandidate);

	PlayerDetectionComponent->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::HandleDetectionBeginOverlap);
	PlayerDetectionComponent->OnComponentEndOverlap.AddUniqueDynamic(this, &ThisClass::HandleDetectionEndOverlap);
	DetectionOverlaps.Seed(PlayerDetectionComponent, IsCandidate);

	const bool bPortalDefinitionReady = LoadedPortalDefinition || ApplyPortalDefinition();
	if (bPortalDefinitionReady && ShouldRenderPortalView())
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

	PortalBoxOverlaps.Reset();
	DetectionOverlaps.Reset();
	TraversalTracker.Reset();

	if (APortalActor* LinkedPortalActor = GetLinkedPortalActor())
	{
		if (USceneCaptureComponent2D* LinkedCapture = LinkedPortalActor->PortalCameraComponent;
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

bool APortalActor::ShouldRenderPortalView() const
{
	return GetNetMode() != NM_DedicatedServer;
}

void APortalActor::NativeTryInitPortalMaterial()
{
	if (!PortalMat)
	{
		UMaterialInterface* MaterialParent = PortalMaterialParent;
		if (!MaterialParent)
		{
			MaterialParent = PortalPlaneComponent->GetMaterial(0);
		}

		if (MaterialParent)
		{
			PortalMat = UMaterialInstanceDynamic::Create(MaterialParent, this);
			PortalPlaneComponent->SetMaterial(0, PortalMat);
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
	const USceneCaptureComponent2D* LinkedCapture = LinkedPortalActor ? LinkedPortalActor->PortalCameraComponent : nullptr;
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
	if (!ShouldRenderPortalView() || !IsCaptureRateLimitElapsed())
	{
		return;
	}

	APortalActor* LinkedPortalActor = GetLinkedPortalActor();
	USceneCaptureComponent2D* LinkedCapture = LinkedPortalActor ? LinkedPortalActor->PortalCameraComponent : nullptr;
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

	const FTransform SourcePortal = GetPortalReferenceTransform();
	const FTransform TargetPortal = LinkedPortalActor->GetPortalReferenceTransform();
	LinkedCapture->SetWorldLocationAndRotation(
		PdPortalSpace::TransformLocation(SourcePortal, TargetPortal, CameraManager->GetCameraLocation()),
		PdPortalSpace::TransformRotation(SourcePortal, TargetPortal, CameraManager->GetCameraRotation()),
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

	TArray<AActor*> OverlappingActors;
	DetectionOverlaps.Collect(
		PlayerDetectionComponent,
		[this](const AActor* Actor) { return IsTeleportCandidate(Actor); },
		OverlappingActors,
		[this](AActor* Actor) { RemoveTraversalStateIfNoLongerOverlapping(Actor); });
	if (OverlappingActors.IsEmpty())
	{
		return false;
	}

	bool bTeleportedAnyActor = false;
	const FVector PlaneLocation = GetPortalPlaneLocation();
	const FVector PlaneNormal = GetPortalForward().GetSafeNormal();
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	for (AActor* Actor : OverlappingActors)
	{
		if (!IsTeleportCandidate(Actor)
			|| !PlayerDetectionComponent->IsOverlappingActor(Actor)
			|| PlaneNormal.IsNearlyZero()
			|| !TraversalTracker.UpdateCrossing(
				Actor, Actor->GetActorLocation(), PlaneLocation, PlaneNormal, Now, TeleportCooldown, CrossingDistanceTolerance))
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
	return ForwardDirectionComponent->GetForwardVector();
}

FVector APortalActor::GetPortalPlaneLocation() const
{
	return PortalPlaneComponent->GetComponentLocation();
}

void APortalActor::HandlePortalBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (PortalBoxOverlaps.Track(OtherActor, [this](const AActor* Actor) { return IsTeleportCandidate(Actor); }))
	{
		SetTickEnabledFromOverlaps();
	}
}

void APortalActor::HandlePortalEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	PortalBoxOverlaps.Untrack(OtherActor, OverlappedComponent);
	RemoveTraversalStateIfNoLongerOverlapping(OtherActor);

	SetTickEnabledFromOverlaps();
}

void APortalActor::HandleDetectionBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (DetectionOverlaps.Track(OtherActor, [this](const AActor* Actor) { return IsTeleportCandidate(Actor); }))
	{
		SetTickEnabledFromOverlaps();
	}
}

void APortalActor::HandleDetectionEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	DetectionOverlaps.Untrack(OtherActor, OverlappedComponent);
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
		if (USceneCaptureComponent2D* LinkedCapture = LinkedPortalActor->PortalCameraComponent)
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

	return LoadedPortalDefinition->GetRenderTargetSize(ViewportSize);
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

	UStaticMesh* PortalMesh = ResolvedDefinition->PortalPlaneMesh.LoadSynchronous();
	UMaterialInterface* PortalMaterial = ResolvedDefinition->PortalMaterial.LoadSynchronous();
	UNiagaraSystem* PortalEffect = ResolvedDefinition->PortalEffect.LoadSynchronous();
	if (!PortalMesh || !PortalMaterial || !PortalEffect)
	{
		LoadedPortalDefinition = nullptr;
		PortalMaterialParent = nullptr;
		return false;
	}

	LoadedPortalDefinition = ResolvedDefinition;
	PortalMaterialParent = PortalMaterial;
	if (PortalPlaneComponent->GetStaticMesh() != PortalMesh)
	{
		PortalPlaneComponent->SetStaticMesh(PortalMesh);
	}
	if (FXComponent->GetAsset() != PortalEffect)
	{
		FXComponent->SetAsset(PortalEffect);
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
	USceneCaptureComponent2D* CaptureComponent = LinkedPortalActor ? LinkedPortalActor->PortalCameraComponent : nullptr;
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

	if (VortexVectorParameterName != NAME_None)
	{
		FXComponent->SetVariableVec3(VortexVectorParameterName, PortalForward);
	}
}

void APortalActor::SetTickEnabledFromOverlaps()
{
	if (!bUpdateOnlyWhenOverlapping)
	{
		SetActorTickEnabled(true);
		return;
	}

	SetActorTickEnabled(PortalBoxOverlaps.HasAny(
		BoxComponent,
		[this](const AActor* Actor) { return IsTeleportCandidate(Actor); }));
}

void APortalActor::RemoveTraversalStateIfNoLongerOverlapping(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	if (!BoxComponent->IsOverlappingActor(Actor) && !PlayerDetectionComponent->IsOverlappingActor(Actor))
	{
		TraversalTracker.Remove(Actor);
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

void APortalActor::TeleportActorThroughPortal(AActor* Actor)
{
	APortalActor* LinkedPortalActor = GetLinkedPortalActor();
	if (!Actor || !LinkedPortalActor)
	{
		return;
	}

	const FTransform SourcePortal = GetPortalReferenceTransform();
	const FTransform TargetPortal = LinkedPortalActor->GetPortalReferenceTransform();
	const FVector TargetLocation = PdPortalSpace::TransformLocation(SourcePortal, TargetPortal, Actor->GetActorLocation())
		+ LinkedPortalActor->GetPortalForward() * ExitOffset;
	const FRotator TargetRotation = PdPortalSpace::TransformRotation(SourcePortal, TargetPortal, Actor->GetActorRotation());

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
		TargetVelocity = PdPortalSpace::TransformVelocity(SourcePortal, TargetPortal, MovementComponent->Velocity);
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
				PdPortalSpace::TransformRotation(SourcePortal, TargetPortal, Controller->GetControlRotation());
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
	if (Actor)
	{
		TraversalTracker.Prime(Actor, Actor->GetActorLocation(), GetPortalPlaneLocation(), GetPortalForward(),
			GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0);
	}
}

FTransform APortalActor::GetPortalReferenceTransform() const
{
	return FTransform(ForwardDirectionComponent->GetComponentQuat(), GetPortalPlaneLocation(), FVector::OneVector);
}
