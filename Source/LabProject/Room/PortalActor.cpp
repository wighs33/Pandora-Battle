#include "Room/PortalActor.h"

#include "Camera/CameraTypes.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
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
#include "NavAreas/NavArea_Obstacle.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PortalActor)

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
	BoxComponent->SetCollisionResponseToChannel(ECC_GameTraceChannel3, ECR_Ignore); // OverlapBox
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

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PortalPlaneMeshAsset(TEXT("/Game/ThirdPerson/Maps/_GENERATED/whgus/disc.disc"));
	if (PortalPlaneMeshAsset.Succeeded())
	{
		PortalPlaneComponent->SetStaticMesh(PortalPlaneMeshAsset.Object);
	}

	ForwardDirectionComponent = CreateDefaultSubobject<UArrowComponent>(TEXT("ForwardDirection"));
	ForwardDirectionComponent->SetupAttachment(PortalPlaneComponent);
	ForwardDirectionComponent->SetRelativeLocation(FVector(-8.0, 0.0, 0.0));
	ForwardDirectionComponent->SetRelativeRotation(FRotator(90.0, 0.0, 0.0));

	FXComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("FX"));
	FXComponent->SetupAttachment(PortalPlaneComponent);
	FXComponent->SetRelativeLocation(FVector(1.422222, 0.0, 0.0));
	FXComponent->SetRelativeRotation(FRotator(90.0, 0.0, 0.0));
	FXComponent->SetRelativeScale3D(FVector(0.2, 0.2, 0.2));

	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> PortalFxAsset(TEXT("/Game/effect/NS_Portal.NS_Portal"));
	if (PortalFxAsset.Succeeded())
	{
		FXComponent->SetAsset(PortalFxAsset.Object);
	}

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

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PortalMaterialAsset(TEXT("/Game/Env/M_Portal.M_Portal"));
	if (PortalMaterialAsset.Succeeded())
	{
		PortalMaterialParent = PortalMaterialAsset.Object;
	}
}

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

	NativeTryInitPortalMaterial();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(InitMaterialTimerHandle, this, &ThisClass::NativeTryInitPortalMaterial, InitRetryInterval, true);
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

	AActor* Actor = ResolveOverlappingTeleportActor();
	if (!Actor)
	{
		return false;
	}

	if (!IsPointCrossingPortal(Actor, Actor->GetActorLocation()))
	{
		return false;
	}

	TeleportActorThroughPortal(Actor);
	return true;
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
	UntrackTeleportOverlap(PortalOverlappingTeleportActors, OtherActor);
	if (OtherActor)
	{
		TraversalStates.Remove(TObjectKey<AActor>(OtherActor));
	}

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
	UntrackTeleportOverlap(DetectedTeleportActors, OtherActor);
	if (OtherActor)
	{
		TraversalStates.Remove(TObjectKey<AActor>(OtherActor));
	}

	SetTickEnabledFromOverlaps();
}

bool APortalActor::EnsureRenderTargetSize()
{
	const FIntPoint DesiredSize = GetDesiredRenderTargetSize();
	if (DesiredSize.X <= 0 || DesiredSize.Y <= 0)
	{
		return false;
	}

	const bool bNeedsNewTarget = !PortalRT;
	const bool bNeedsResize = PortalRT && (PortalRT->SizeX != DesiredSize.X || PortalRT->SizeY != DesiredSize.Y);

	if (!bNeedsNewTarget && !bNeedsResize)
	{
		return true;
	}

	if (!PortalRT)
	{
		PortalRT = NewObject<UTextureRenderTarget2D>(this, TEXT("PortalRT"));
	}

	PortalRT->RenderTargetFormat = RTF_RGBA16f;
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
	if (GEngine && GEngine->GameViewport)
	{
		FVector2D ViewportSize = FVector2D::ZeroVector;
		GEngine->GameViewport->GetViewportSize(ViewportSize);

		const int32 ViewportX = FMath::TruncToInt(ViewportSize.X);
		const int32 ViewportY = FMath::TruncToInt(ViewportSize.Y);
		if (ViewportX > 0 && ViewportY > 0)
		{
			return FIntPoint(ViewportX, ViewportY);
		}
	}

	return FIntPoint(FMath::Max(FallbackRenderTargetSize.X, 16), FMath::Max(FallbackRenderTargetSize.Y, 16));
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

	SetActorTickEnabled(HasTrackedTeleportOverlap(PortalOverlappingTeleportActors));
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

void APortalActor::UntrackTeleportOverlap(TArray<TWeakObjectPtr<AActor>>& OverlappingActors, AActor* Actor) const
{
	if (!Actor)
	{
		return;
	}

	OverlappingActors.RemoveAllSwap(
		[Actor](const TWeakObjectPtr<AActor>& ExistingActor)
		{
			return !ExistingActor.IsValid() || ExistingActor.Get() == Actor;
		});
}

bool APortalActor::HasTrackedTeleportOverlap(TArray<TWeakObjectPtr<AActor>>& OverlappingActors) const
{
	for (int32 ActorIndex = OverlappingActors.Num() - 1; ActorIndex >= 0; --ActorIndex)
	{
		AActor* Actor = OverlappingActors[ActorIndex].Get();
		if (!IsTeleportCandidate(Actor))
		{
			OverlappingActors.RemoveAtSwap(ActorIndex);
			continue;
		}

		return true;
	}

	return false;
}

bool APortalActor::IsTeleportCandidate(const AActor* Actor) const
{
	if (!IsValid(Actor) || Actor == this || Actor == GetLinkedPortalActor())
	{
		return false;
	}

	return !TeleportableActorClass || Actor->IsA(TeleportableActorClass);
}

AActor* APortalActor::ResolveOverlappingTeleportActor()
{
	for (int32 ActorIndex = DetectedTeleportActors.Num() - 1; ActorIndex >= 0; --ActorIndex)
	{
		AActor* Actor = DetectedTeleportActors[ActorIndex].Get();
		if (IsTeleportCandidate(Actor))
		{
			return Actor;
		}

		DetectedTeleportActors.RemoveAtSwap(ActorIndex);
	}

	return nullptr;
}

APortalActor* APortalActor::GetLinkedPortalActor() const
{
	if (LinkedPortal)
	{
		return LinkedPortal;
	}

	const FObjectPropertyBase* LinkedPortalProperty = FindFProperty<FObjectPropertyBase>(GetClass(), TEXT("LinkedPortal"));
	if (!LinkedPortalProperty)
	{
		return nullptr;
	}

	return Cast<APortalActor>(LinkedPortalProperty->GetObjectPropertyValue_InContainer(this));
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
			Controller->SetControlRotation(TransformRotationToLinkedPortal(Controller->GetControlRotation()));
		}
	}

	if (MovementComponent)
	{
		MovementComponent->Velocity = TargetVelocity;
	}

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
