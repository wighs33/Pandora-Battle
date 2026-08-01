#include "Component/Player/GrappleComponent.h"

#include "AbilitySystemComponent.h"

#include "AbilitySystem/TargetValidator.h"
#include "CableComponent.h"
#include "Character/PdPlayer.h"
#include "Common/LabGameplayTags.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GrappleComponent)

namespace
{
	const FName GrappleHookSocketName(TEXT("hand_r"));
}

UGrappleComponent::UGrappleComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	SetIsReplicatedByDefault(true);
}

void UGrappleComponent::BeginPlay()
{
	Super::BeginPlay();
	SetComponentTickEnabled(false);
	ConfigureHookComponent();
}

void UGrappleComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopGrapple();
	Super::EndPlay(EndPlayReason);
}

void UGrappleComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	APdPlayer* PlayerOwner = GetPlayerOwner();
	UCharacterMovementComponent* MovementComponent =
		PlayerOwner ? PlayerOwner->GetCharacterMovement() : nullptr;
	if (!bGrappleMoveActive
		|| !bIsGrappling
		|| !PlayerOwner
		|| !PlayerOwner->HasAuthority()
		|| !MovementComponent
		|| !MovementComponent->UpdatedComponent)
	{
		if (bGrappleMoveActive)
		{
			StopGrapple();
		}
		return;
	}

	const double SafeMoveDuration = FMath::Max(MoveDuration, UE_KINDA_SMALL_NUMBER);
	GrappleMoveElapsedTime += FMath::Max(static_cast<double>(DeltaTime), 0.0);
	const double MoveAlpha = FMath::Clamp(GrappleMoveElapsedTime / SafeMoveDuration, 0.0, 1.0);
	const FVector DesiredLocation = FMath::Lerp(
		GrappleMoveStartLocation,
		GrappleMoveDestination,
		MoveAlpha);
	const FQuat DesiredRotation = FQuat::Slerp(
		GrappleMoveStartRotation.Quaternion(),
		GrappleMoveTargetRotation.Quaternion(),
		MoveAlpha);

	FHitResult MoveHit;
	MovementComponent->SafeMoveUpdatedComponent(
		DesiredLocation - PlayerOwner->GetActorLocation(),
		DesiredRotation,
		true,
		MoveHit);

	if (IsCapsuleLocationClear(PlayerOwner->GetActorLocation()))
	{
		LastSafeGrappleLocation = PlayerOwner->GetActorLocation();
	}

	const bool bBlocked = MoveHit.bStartPenetrating || MoveHit.IsValidBlockingHit();
	if (bBlocked || MoveAlpha >= 1.0)
	{
		FinishGrapple();
	}
}

void UGrappleComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(UGrappleComponent, bIsGrappling, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UGrappleComponent, PendingGrappleHitComponent, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UGrappleComponent, PendingGrappleImpactPoint, Params);
}

void UGrappleComponent::SetHookComponent(UCableComponent* InHookComponent)
{
	HookComponent = InHookComponent;
	ConfigureHookComponent();
}

bool UGrappleComponent::TraceGrappleFromView(
	const FVector& ViewLocation,
	const FVector& ViewDirection,
	FHitResult& OutHitResult) const
{
	APdPlayer* PlayerOwner = GetPlayerOwner();
	if (!PlayerOwner || ViewLocation.ContainsNaN() || ViewDirection.ContainsNaN())
	{
		return false;
	}

	FVector TraceDirection = ViewDirection;
	if (TraceDirection.IsNearlyZero())
	{
		return false;
	}

	TraceDirection.Normalize();
	const FVector TraceStart = ViewLocation + TraceDirection * TraceStartOffset;
	const FVector TraceEnd = TraceStart + TraceDirection * TraceDistance;

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(PlayerOwner);

	const bool bHit = UKismetSystemLibrary::SphereTraceSingle(
		PlayerOwner,
		TraceStart,
		TraceEnd,
		static_cast<float>(TraceRadius),
		TraceChannel.GetValue(),
		false,
		ActorsToIgnore,
		bDrawTraceDebug ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None,
		OutHitResult,
		true);

	return bHit && OutHitResult.GetComponent();
}

bool UGrappleComponent::ValidateTargetDataAndTrace(
	const FHitResult& ClientHitResult,
	FHitResult& OutServerHitResult) const
{
	const APdPlayer* PlayerOwner = GetPlayerOwner();
	if (!PlayerOwner)
	{
		return false;
	}

	PdTargetValidator::FTraceRequestValidationParams ValidationParams;
	ValidationParams.TraceStartOffset = TraceStartOffset;
	ValidationParams.MaxTraceDistance = TraceDistance;

	PdTargetValidator::FValidatedTraceView ValidatedView;
	if (!PdTargetValidator::ValidateClientTraceRequest(
		PlayerOwner,
		ClientHitResult,
		ValidationParams,
		ValidatedView))
	{
		return false;
	}

	return TraceGrappleFromView(
		ValidatedView.ViewLocation,
		ValidatedView.ViewDirection,
		OutServerHitResult);
}

bool UGrappleComponent::StartGrappleFromValidatedHit(const FHitResult& HitResult)
{
	APdPlayer* PlayerOwner = GetPlayerOwner();
	const UAbilitySystemComponent* AbilitySystemComponent =
		PlayerOwner ? PlayerOwner->GetAbilitySystemComponent() : nullptr;
	if (!PlayerOwner
		|| !PlayerOwner->HasAuthority()
		|| bIsGrappling
		|| (AbilitySystemComponent
			&& AbilitySystemComponent->HasMatchingGameplayTag(LabGameplayTags::Status_Frostbite)))
	{
		return false;
	}

	UPrimitiveComponent* HitComponent = HitResult.GetComponent();
	if (!HitComponent)
	{
		return false;
	}

	FVector SafeGrappleDestination = FVector::ZeroVector;
	if (!ResolveSafeGrappleDestination(HitResult, SafeGrappleDestination))
	{
		return false;
	}

	GrappleMoveDestination = SafeGrappleDestination;
	SetGrappleState(true);
	SetGrappleTarget(HitComponent, HitResult.ImpactPoint);
	OnGrappleStarted.Broadcast();

	UWorld* World = GetWorld();
	if (World && HookAttachDelay > 0.0)
	{
		World->GetTimerManager().SetTimer(
			GrappleHookAttachTimerHandle,
			this,
			&ThisClass::AttachGrappleHookToTarget,
			static_cast<float>(HookAttachDelay),
			false);
	}
	else
	{
		AttachGrappleHookToTarget();
	}

	if (World && MoveStartDelay > 0.0)
	{
		World->GetTimerManager().SetTimer(
			GrappleMoveStartTimerHandle,
			this,
			&ThisClass::StartGrappleMove,
			static_cast<float>(MoveStartDelay),
			false);
	}
	else
	{
		StartGrappleMove();
	}

	return true;
}

void UGrappleComponent::StopGrapple()
{
	const bool bWasGrappling = bIsGrappling;
	const bool bWasGrappleMoveActive = bGrappleMoveActive;

	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(GrappleMoveStartTimerHandle);
		World->GetTimerManager().ClearTimer(GrappleHookAttachTimerHandle);
	}

	GrappleMoveStartTimerHandle.Invalidate();
	GrappleHookAttachTimerHandle.Invalidate();

	if (bGrappleMoveActive)
	{
		RecoverPlayerFromPenetration();
	}
	RestoreMovementAfterGrapple();

	APdPlayer* PlayerOwner = GetPlayerOwner();
	if (bWasGrappleMoveActive && PlayerOwner && PlayerOwner->HasAuthority())
	{
		const UCharacterMovementComponent* MovementComponent = PlayerOwner->GetCharacterMovement();
		ClientCorrectGrappleEnd(
			PlayerOwner->GetActorLocation(),
			MovementComponent ? static_cast<uint8>(MovementComponent->MovementMode) : static_cast<uint8>(MOVE_Falling),
			MovementComponent ? MovementComponent->CustomMovementMode : 0);
	}
	ResetGrappleMovementState();

	SetGrappleState(false);
	ClearGrappleTarget();
	ResetGrappleHookVisual();

	if (PlayerOwner && PlayerOwner->HasAuthority())
	{
		PlayerOwner->ForceNetUpdate();
	}

	if (bWasGrappling)
	{
		OnGrappleFinished.Broadcast();
	}
}

void UGrappleComponent::ResetForRespawn()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(GrappleMoveStartTimerHandle);
		World->GetTimerManager().ClearTimer(GrappleHookAttachTimerHandle);
	}

	GrappleMoveStartTimerHandle.Invalidate();
	GrappleHookAttachTimerHandle.Invalidate();
	ResetGrappleMovementState();
	bMovementModeOverridden = false;

	SetGrappleState(false);
	ClearGrappleTarget();
	ConfigureHookComponent();

	if (APdPlayer* PlayerOwner = GetPlayerOwner(); PlayerOwner && PlayerOwner->HasAuthority())
	{
		PlayerOwner->ForceNetUpdate();
	}
}

void UGrappleComponent::FinishGrapple()
{
	StopGrapple();
}

void UGrappleComponent::AttachGrappleHookToTarget()
{
	UPrimitiveComponent* HitComponent = PendingGrappleHitComponent.Get();
	if (!bIsGrappling || !HookComponent || !IsValid(HitComponent))
	{
		return;
	}

	HookComponent->CableLength = FMath::Max(
		static_cast<float>(FVector::Distance(HookComponent->GetComponentLocation(), PendingGrappleImpactPoint)),
		1.0f);
	HookComponent->SetAttachEndToComponent(HitComponent, NAME_None);
	HookComponent->EndLocation = HitComponent->GetComponentTransform().InverseTransformPosition(PendingGrappleImpactPoint);
	HookComponent->bAttachEnd = true;
	HookComponent->SetHiddenInGame(false);
	HookComponent->SetVisibility(true, true);
}

void UGrappleComponent::StartGrappleMove()
{
	APdPlayer* PlayerOwner = GetPlayerOwner();
	if (!PlayerOwner
		|| !PlayerOwner->HasAuthority()
		|| !bIsGrappling
		|| !IsValid(PendingGrappleHitComponent.Get())
		|| GrappleMoveDestination.ContainsNaN())
	{
		StopGrapple();
		return;
	}

	UCharacterMovementComponent* MovementComponent = PlayerOwner->GetCharacterMovement();
	UCapsuleComponent* PlayerCapsuleComponent = PlayerOwner->GetCapsuleComponent();
	if (!MovementComponent || !MovementComponent->UpdatedComponent || !PlayerCapsuleComponent)
	{
		StopGrapple();
		return;
	}

	if (!IsCapsuleLocationClear(GrappleMoveDestination))
	{
		StopGrapple();
		return;
	}

	GrappleMoveStartLocation = PlayerOwner->GetActorLocation();
	LastSafeGrappleLocation = GrappleMoveStartLocation;
	GrappleMoveStartRotation = PlayerOwner->GetActorRotation();
	const FVector DirectionToTarget = GrappleMoveDestination - GrappleMoveStartLocation;
	GrappleMoveTargetRotation = FRotator(
		0.0,
		DirectionToTarget.Rotation().Yaw,
		0.0);
	GrappleMoveElapsedTime = 0.0;

	MovementModeBeforeGrapple = MovementComponent->MovementMode;
	CustomMovementModeBeforeGrapple = MovementComponent->CustomMovementMode;
	bMovementModeOverridden = true;
	MovementComponent->StopMovementImmediately();
	MovementComponent->DisableMovement();

	bGrappleMoveActive = true;
	SetComponentTickEnabled(true);
	PlayerOwner->ForceNetUpdate();
}

void UGrappleComponent::OnRep_IsGrappling(const bool bWasGrappling)
{
	ApplyReplicatedGrappleState();

	if (!bWasGrappling && bIsGrappling)
	{
		OnGrappleStarted.Broadcast();
	}
	else if (bWasGrappling && !bIsGrappling)
	{
		OnGrappleFinished.Broadcast();
	}
}

void UGrappleComponent::OnRep_GrappleTarget()
{
	ApplyReplicatedGrappleState();
}

void UGrappleComponent::ClientCorrectGrappleEnd_Implementation(
	const FVector_NetQuantize100 ServerLocation,
	const uint8 ServerMovementMode,
	const uint8 ServerCustomMovementMode)
{
	APdPlayer* PlayerOwner = GetPlayerOwner();
	if (!PlayerOwner || PlayerOwner->HasAuthority() || !PlayerOwner->IsLocallyControlled())
	{
		return;
	}

	PlayerOwner->SetActorLocation(
		ServerLocation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);

	if (UCharacterMovementComponent* MovementComponent = PlayerOwner->GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
		const EMovementMode CorrectedMovementMode =
			ServerMovementMode < static_cast<uint8>(MOVE_MAX)
				? static_cast<EMovementMode>(ServerMovementMode)
				: MOVE_Falling;
		MovementComponent->SetMovementMode(
			CorrectedMovementMode,
			ServerCustomMovementMode);
	}
}

void UGrappleComponent::ConfigureHookComponent()
{
	if (!HookComponent)
	{
		return;
	}

	if (const APdPlayer* PlayerOwner = GetPlayerOwner())
	{
		if (USkeletalMeshComponent* MeshComponent = PlayerOwner->GetMesh())
		{
			const bool bNeedsReattach = HookComponent->GetAttachParent() != MeshComponent
				|| HookComponent->GetAttachSocketName() != GrappleHookSocketName;
			if (bNeedsReattach)
			{
				HookComponent->AttachToComponent(
					MeshComponent,
					FAttachmentTransformRules::SnapToTargetNotIncludingScale,
					GrappleHookSocketName);
			}
		}
	}

	HookComponent->SetUsingAbsoluteLocation(false);
	HookComponent->SetUsingAbsoluteRotation(false);
	HookComponent->SetUsingAbsoluteScale(false);
	HookComponent->SetRelativeLocation(FVector::ZeroVector);
	HookComponent->SetRelativeRotation(FRotator::ZeroRotator);
	HookComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HookComponent->SetGenerateOverlapEvents(false);
	HookComponent->SetHiddenInGame(true);
	HookComponent->SetVisibility(false, true);
	HookComponent->bAttachStart = true;
	HookComponent->bAttachEnd = false;
	HookComponent->CableLength = 0.0f;
	HookComponent->EndLocation = FVector::ZeroVector;
}

void UGrappleComponent::SetGrappleState(const bool bNewIsGrappling)
{
	if (bIsGrappling == bNewIsGrappling)
	{
		return;
	}

	bIsGrappling = bNewIsGrappling;
	MarkGrappleReplicationDirty();
}

void UGrappleComponent::SetGrappleTarget(UPrimitiveComponent* HitComponent, const FVector& ImpactPoint)
{
	PendingGrappleHitComponent = HitComponent;
	PendingGrappleImpactPoint = ImpactPoint;
	MarkGrappleReplicationDirty();

	if (APdPlayer* PlayerOwner = GetPlayerOwner(); PlayerOwner && PlayerOwner->HasAuthority())
	{
		PlayerOwner->ForceNetUpdate();
	}
}

void UGrappleComponent::ClearGrappleTarget()
{
	PendingGrappleHitComponent = nullptr;
	PendingGrappleImpactPoint = FVector::ZeroVector;
	MarkGrappleReplicationDirty();
}

void UGrappleComponent::MarkGrappleReplicationDirty()
{
	const APdPlayer* PlayerOwner = GetPlayerOwner();
	if (!PlayerOwner || !PlayerOwner->HasAuthority())
	{
		return;
	}

	MARK_PROPERTY_DIRTY_FROM_NAME(UGrappleComponent, bIsGrappling, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(UGrappleComponent, PendingGrappleHitComponent, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(UGrappleComponent, PendingGrappleImpactPoint, this);
}

void UGrappleComponent::ApplyReplicatedGrappleState()
{
	UWorld* World = GetWorld();
	if (!bIsGrappling)
	{
		if (World)
		{
			World->GetTimerManager().ClearTimer(GrappleHookAttachTimerHandle);
		}
		GrappleHookAttachTimerHandle.Invalidate();
		ResetGrappleHookVisual();
		return;
	}

	if (!HookComponent || !PendingGrappleHitComponent.Get())
	{
		return;
	}

	if (HookComponent->IsVisible() || HookAttachDelay <= 0.0)
	{
		AttachGrappleHookToTarget();
		return;
	}

	if (World && !World->GetTimerManager().IsTimerActive(GrappleHookAttachTimerHandle))
	{
		World->GetTimerManager().SetTimer(
			GrappleHookAttachTimerHandle,
			this,
			&ThisClass::AttachGrappleHookToTarget,
			static_cast<float>(HookAttachDelay),
			false);
	}
}

void UGrappleComponent::ResetGrappleHookVisual()
{
	if (HookComponent)
	{
		HookComponent->SetAttachEndToComponent(nullptr, NAME_None);
		HookComponent->bAttachEnd = false;
		HookComponent->CableLength = 0.0f;
		HookComponent->EndLocation = FVector::ZeroVector;
		HookComponent->SetHiddenInGame(true);
		HookComponent->SetVisibility(false, true);
	}
}

bool UGrappleComponent::ResolveSafeGrappleDestination(
	const FHitResult& HitResult,
	FVector& OutDestination) const
{
	const APdPlayer* PlayerOwner = GetPlayerOwner();
	const UCapsuleComponent* CapsuleComponent =
		PlayerOwner ? PlayerOwner->GetCapsuleComponent() : nullptr;
	UWorld* World = GetWorld();
	if (!PlayerOwner
		|| !CapsuleComponent
		|| !World
		|| HitResult.ImpactPoint.ContainsNaN())
	{
		return false;
	}

	FVector SurfaceNormal = HitResult.ImpactNormal.GetSafeNormal();
	if (SurfaceNormal.IsNearlyZero())
	{
		SurfaceNormal = (PlayerOwner->GetActorLocation() - HitResult.ImpactPoint).GetSafeNormal();
	}
	if (SurfaceNormal.IsNearlyZero())
	{
		SurfaceNormal = FVector::UpVector;
	}

	const double CapsuleRadius = CapsuleComponent->GetScaledCapsuleRadius();
	const double CapsuleHalfHeight = CapsuleComponent->GetScaledCapsuleHalfHeight();
	const double CapsuleSupportDistance = CapsuleRadius
		+ FMath::Max(CapsuleHalfHeight - CapsuleRadius, 0.0) * FMath::Abs(SurfaceNormal.Z);
	const double SafeClearance = FMath::Max(EndpointClearance, 0.0);
	FVector CandidateLocation = HitResult.ImpactPoint
		+ SurfaceNormal * (CapsuleSupportDistance + SafeClearance);

	if (!CandidateLocation.ContainsNaN() && IsCapsuleLocationClear(CandidateLocation))
	{
		OutDestination = CandidateLocation;
		return true;
	}

	FRotator CandidateRotation(0.0, PlayerOwner->GetActorRotation().Yaw, 0.0);
	if (!CandidateLocation.ContainsNaN()
		&& World->FindTeleportSpot(PlayerOwner, CandidateLocation, CandidateRotation)
		&& IsCapsuleLocationClear(CandidateLocation))
	{
		OutDestination = CandidateLocation;
		return true;
	}

	return false;
}

bool UGrappleComponent::IsCapsuleLocationClear(const FVector& Location) const
{
	const APdPlayer* PlayerOwner = GetPlayerOwner();
	const UCapsuleComponent* CapsuleComponent =
		PlayerOwner ? PlayerOwner->GetCapsuleComponent() : nullptr;
	const UWorld* World = GetWorld();
	if (!PlayerOwner
		|| !CapsuleComponent
		|| !World
		|| Location.ContainsNaN())
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GrappleSafeLocation), false, PlayerOwner);
	const FCollisionResponseParams ResponseParams(CapsuleComponent->GetCollisionResponseToChannels());
	const FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(
		CapsuleComponent->GetScaledCapsuleRadius(),
		CapsuleComponent->GetScaledCapsuleHalfHeight());
	return !World->OverlapBlockingTestByChannel(
		Location,
		CapsuleComponent->GetComponentQuat(),
		CapsuleComponent->GetCollisionObjectType(),
		CapsuleShape,
		QueryParams,
		ResponseParams);
}

bool UGrappleComponent::RecoverPlayerFromPenetration()
{
	APdPlayer* PlayerOwner = GetPlayerOwner();
	UWorld* World = GetWorld();
	if (!PlayerOwner || !PlayerOwner->HasAuthority() || !World)
	{
		return false;
	}

	if (IsCapsuleLocationClear(PlayerOwner->GetActorLocation()))
	{
		return true;
	}

	const FVector RecoveryCandidates[] =
	{
		LastSafeGrappleLocation,
		GrappleMoveStartLocation
	};
	for (const FVector& RecoveryCandidate : RecoveryCandidates)
	{
		if (RecoveryCandidate.ContainsNaN() || !IsCapsuleLocationClear(RecoveryCandidate))
		{
			continue;
		}

		PlayerOwner->SetActorLocation(
			RecoveryCandidate,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
		return true;
	}

	FVector TeleportCandidate = GrappleMoveStartLocation;
	const FRotator TeleportRotation(0.0, PlayerOwner->GetActorRotation().Yaw, 0.0);
	if (!TeleportCandidate.ContainsNaN()
		&& World->FindTeleportSpot(PlayerOwner, TeleportCandidate, TeleportRotation)
		&& IsCapsuleLocationClear(TeleportCandidate))
	{
		PlayerOwner->SetActorLocation(
			TeleportCandidate,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
		return true;
	}

	return false;
}

void UGrappleComponent::RestoreMovementAfterGrapple()
{
	APdPlayer* PlayerOwner = GetPlayerOwner();
	UCharacterMovementComponent* MovementComponent =
		PlayerOwner ? PlayerOwner->GetCharacterMovement() : nullptr;
	if (!bMovementModeOverridden || !MovementComponent)
	{
		bMovementModeOverridden = false;
		return;
	}

	MovementComponent->StopMovementImmediately();
	switch (MovementModeBeforeGrapple.GetValue())
	{
	case MOVE_Swimming:
	case MOVE_Flying:
	case MOVE_Custom:
		MovementComponent->SetMovementMode(
			MovementModeBeforeGrapple.GetValue(),
			CustomMovementModeBeforeGrapple);
		break;
	default:
		MovementComponent->SetMovementMode(MOVE_Falling);
		break;
	}
	bMovementModeOverridden = false;
}

void UGrappleComponent::ResetGrappleMovementState()
{
	SetComponentTickEnabled(false);
	bGrappleMoveActive = false;
	GrappleMoveStartLocation = FVector::ZeroVector;
	GrappleMoveDestination = FVector::ZeroVector;
	LastSafeGrappleLocation = FVector::ZeroVector;
	GrappleMoveStartRotation = FRotator::ZeroRotator;
	GrappleMoveTargetRotation = FRotator::ZeroRotator;
	GrappleMoveElapsedTime = 0.0;
}

APdPlayer* UGrappleComponent::GetPlayerOwner() const
{
	return Cast<APdPlayer>(GetOwner());
}
