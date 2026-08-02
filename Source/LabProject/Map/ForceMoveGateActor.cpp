#include "Map/ForceMoveGateActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Mode/PdPlayerController.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ForceMoveGateActor)

AForceMoveGateActor::AForceMoveGateActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	bReplicates = true;
	bAlwaysRelevant = true;
	SetCanBeDamaged(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	GateMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GateMesh"));
	GateMesh->SetupAttachment(SceneRoot);
	GateMesh->SetMobility(EComponentMobility::Movable);
	GateMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GateMesh->SetCollisionObjectType(ECC_WorldDynamic);
	GateMesh->SetCollisionResponseToAllChannels(ECR_Block);
}

void AForceMoveGateActor::BeginPlay()
{
	Super::BeginPlay();

	if (GateMesh)
	{
		ClosedRelativeLocation = GateMesh->GetRelativeLocation();
		bInitializedClosedLocation = true;
	}

	if (HasAuthority())
	{
		bGateRaised = bStartRaised;
		MARK_PROPERTY_DIRTY_FROM_NAME(AForceMoveGateActor, bGateRaised, this);
	}

	ApplyGateRaisedStateImmediately();
}

void AForceMoveGateActor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bMovementActive || !GateMesh)
	{
		SetActorTickEnabled(false);
		return;
	}

	if (RaiseDuration <= KINDA_SMALL_NUMBER)
	{
		FinishGateMovement();
		return;
	}

	MovementElapsedSeconds += DeltaSeconds;
	const float Alpha = FMath::Clamp(MovementElapsedSeconds / RaiseDuration, 0.0f, 1.0f);
	const float SmoothAlpha = FMath::InterpEaseInOut(0.0f, 1.0f, Alpha, 2.0f);
	GateMesh->SetRelativeLocation(FMath::Lerp(MovementStartRelativeLocation, MovementTargetRelativeLocation, SmoothAlpha));

	if (Alpha >= 1.0f)
	{
		FinishGateMovement();
	}
}

void AForceMoveGateActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(AForceMoveGateActor, bGateRaised, Params);
}

void AForceMoveGateActor::HandleForceMoveTriggered(APdPlayerController* TriggeringPlayerController)
{
	if (!HasAuthority())
	{
		return;
	}

	if (!bRaiseWhenForceMoveTriggered)
	{
		return;
	}

	if (!bGateRaised)
	{
		bGateRaised = true;
		MARK_PROPERTY_DIRTY_FROM_NAME(AForceMoveGateActor, bGateRaised, this);
		ForceNetUpdate();
		Multicast_PlayGateMovement(true, TriggeringPlayerController);
	}

}

void AForceMoveGateActor::Multicast_PlayGateMovement_Implementation(
	const bool bInRaised,
	APdPlayerController* TriggeringPlayerController)
{
	StartGateMovement(bInRaised, TriggeringPlayerController);
}

void AForceMoveGateActor::OnRep_GateRaised()
{
	const FVector DesiredTargetLocation = bGateRaised ? GetRaisedRelativeLocation() : ClosedRelativeLocation;
	if (bMovementActive && MovementTargetRelativeLocation.Equals(DesiredTargetLocation))
	{
		return;
	}

	StartGateMovement(bGateRaised, nullptr);
}

void AForceMoveGateActor::ApplyGateRaisedStateImmediately()
{
	if (!GateMesh)
	{
		return;
	}

	if (!bInitializedClosedLocation)
	{
		ClosedRelativeLocation = GateMesh->GetRelativeLocation();
		bInitializedClosedLocation = true;
	}

	bMovementActive = false;
	SetActorTickEnabled(false);
	GateMesh->SetRelativeLocation(bGateRaised ? GetRaisedRelativeLocation() : ClosedRelativeLocation);
}

void AForceMoveGateActor::StartGateMovement(
	const bool bInRaised,
	APdPlayerController* TriggeringPlayerController)
{
	if (!GateMesh)
	{
		return;
	}

	if (!bInitializedClosedLocation)
	{
		ClosedRelativeLocation = GateMesh->GetRelativeLocation();
		bInitializedClosedLocation = true;
	}

	MovementStartRelativeLocation = GateMesh->GetRelativeLocation();
	MovementTargetRelativeLocation = bInRaised ? GetRaisedRelativeLocation() : ClosedRelativeLocation;
	MovementElapsedSeconds = 0.0f;

	if (RaiseDuration <= KINDA_SMALL_NUMBER)
	{
		FinishGateMovement();
	}
	else
	{
		bMovementActive = true;
		SetActorTickEnabled(true);
	}

	if (bInRaised)
	{
		BP_OnGateRaiseStarted(TriggeringPlayerController);
	}
	else
	{
		BP_OnGateLowerStarted();
	}

}

void AForceMoveGateActor::FinishGateMovement()
{
	if (GateMesh)
	{
		GateMesh->SetRelativeLocation(MovementTargetRelativeLocation);
	}

	bMovementActive = false;
	SetActorTickEnabled(false);
}

FVector AForceMoveGateActor::GetRaisedRelativeLocation() const
{
	return ClosedRelativeLocation + RaiseOffset;
}
