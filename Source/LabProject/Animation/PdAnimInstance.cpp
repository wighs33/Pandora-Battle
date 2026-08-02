#include "Animation/PdAnimInstance.h"
#include "Character/CharacterBase.h"
#include "Character/PdPlayer.h"
#include "Common/WeaponAnimNotifyNames.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "KismetAnimationLibrary.h"
#include "Component/Player/EquipmentComponent.h"
#include "Weapon/WeaponBase.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(PdAnimInstance)

DEFINE_LOG_CATEGORY(CommonAnimInstanceLog);

void UPdAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	RefreshGameThreadReferences(true);
	CaptureGameThreadSnapshot();
	UpdateGrappleState();
}

void UPdAnimInstance::NativeUninitializeAnimation()
{
	check(IsInGameThread());

	CachedCharacter = nullptr;
	MovementComponent = nullptr;
	++GameThreadSourceRevision;
	GameThreadSnapshot = FGameThreadSnapshot();
	GameThreadSnapshot.SourceRevision = GameThreadSourceRevision;
	ResetThreadSafeAnimationData();
	bIsGrappling = false;

	Super::NativeUninitializeAnimation();
}

void UPdAnimInstance::NativeUpdateAnimation(const float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	RefreshGameThreadReferences();
	CaptureGameThreadSnapshot();
	UpdateGrappleState();
}

void UPdAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

	// NativeUpdateAnimation finishes before this phase. Copy the value-only
	// snapshot once so the worker path never follows a UObject pointer.
	const FGameThreadSnapshot Snapshot = GameThreadSnapshot;

	if (!Snapshot.bIsValid)
	{
		ResetThreadSafeAnimationData();
		LastProcessedSourceRevision = Snapshot.SourceRevision;
		return;
	}

	if (LastProcessedSourceRevision != Snapshot.SourceRevision)
	{
		PreviousVelocity = Snapshot.Velocity;
		bHasPreviousVelocity = false;
		LastProcessedSourceRevision = Snapshot.SourceRevision;
	}

	UpdateLocationData(Snapshot);
	UpdateMovementStates(Snapshot);
	UpdateAcceleration(DeltaSeconds);
	UpdateAimingData(Snapshot);
}

void UPdAnimInstance::AnimNotify_RedrawBow()
{
	check(IsInGameThread());

	APdPlayer* PlayerCharacter = Cast<APdPlayer>(CachedCharacter.Get());
	if (!PlayerCharacter)
	{
		PlayerCharacter = Cast<APdPlayer>(GetOwningActor());
	}

	if (!PlayerCharacter)
	{
		return;
	}

	UEquipmentComponent* EquipmentComponent = PlayerCharacter->GetEquipmentComponent();
	AWeaponBase* WeaponActor = EquipmentComponent ? EquipmentComponent->GetCurrentWeaponActor() : nullptr;
	if (!WeaponActor)
	{
		return;
	}

	WeaponActor->OnWeaponAnimNotifyTiming(WeaponAnimNotifyNames::RedrawBow(), PlayerCharacter);
}

void UPdAnimInstance::RefreshGameThreadReferences(const bool bForceNewRevision)
{
	check(IsInGameThread());

	ACharacter* NewCharacter = Cast<ACharacter>(GetOwningActor());
	if (!IsValid(NewCharacter))
	{
		NewCharacter = nullptr;
	}

	UCharacterMovementComponent* NewMovementComponent =
		NewCharacter ? NewCharacter->GetCharacterMovement() : nullptr;
	if (!IsValid(NewMovementComponent))
	{
		NewMovementComponent = nullptr;
	}

	if (bForceNewRevision ||
		CachedCharacter.Get() != NewCharacter ||
		MovementComponent.Get() != NewMovementComponent)
	{
		++GameThreadSourceRevision;
	}

	CachedCharacter = NewCharacter;
	MovementComponent = NewMovementComponent;
}

void UPdAnimInstance::CaptureGameThreadSnapshot()
{
	check(IsInGameThread());

	FGameThreadSnapshot NewSnapshot;
	NewSnapshot.SourceRevision = GameThreadSourceRevision;

	ACharacter* Character = CachedCharacter.Get();
	UCharacterMovementComponent* CharacterMovement = MovementComponent.Get();
	if (!IsValid(Character) || !IsValid(CharacterMovement))
	{
		GameThreadSnapshot = NewSnapshot;
		return;
	}

	const FVector CurrentVelocity = CharacterMovement->Velocity;
	NewSnapshot.Velocity = FVector3f(CurrentVelocity);
	NewSnapshot.Direction = UKismetAnimationLibrary::CalculateDirection(
		CurrentVelocity,
		Character->GetActorRotation());
	NewSnapshot.bIsFalling = CharacterMovement->IsFalling();
	NewSnapshot.bIsOnGround = CharacterMovement->IsMovingOnGround();
	NewSnapshot.bIsCrouching = CharacterMovement->IsCrouching();

	if (const ACharacterBase* PdCharacter = Cast<ACharacterBase>(Character))
	{
		NewSnapshot.AimYaw = PdCharacter->GetAimYawForAnimation();
		NewSnapshot.AimPitch = PdCharacter->GetAimPitchForAnimation();
	}
	else
	{
		const FRotator AimDelta =
			(Character->GetBaseAimRotation() - Character->GetActorRotation()).GetNormalized();
		NewSnapshot.AimYaw = AimDelta.Yaw;
		NewSnapshot.AimPitch = AimDelta.Pitch;
	}

	NewSnapshot.bIsValid = true;
	GameThreadSnapshot = NewSnapshot;
}

void UPdAnimInstance::ResetThreadSafeAnimationData()
{
	Velocity = FVector3f::ZeroVector;
	GroundSpeed = 0.f;
	Direction = 0.f;
	VerticalVelocity = 0.f;
	Acceleration = FVector3f::ZeroVector;

	bIsMoving = false;
	bIsFalling = false;
	bIsJumping = false;
	bIsCrouching = false;
	bIsOnGround = true;

	AimYaw = 0.f;
	AimPitch = 0.f;

	PreviousVelocity = FVector3f::ZeroVector;
	bHasPreviousVelocity = false;
}

void UPdAnimInstance::UpdateLocationData(const FGameThreadSnapshot& Snapshot)
{
	Velocity = Snapshot.Velocity;
	GroundSpeed = Velocity.Size2D();
	Direction = Snapshot.Direction;
	VerticalVelocity = Velocity.Z;
}

void UPdAnimInstance::UpdateMovementStates(const FGameThreadSnapshot& Snapshot)
{
	bIsMoving = GroundSpeed > MovingSpeedThreshold;
	bIsFalling = Snapshot.bIsFalling;
	bIsJumping = bIsFalling && Velocity.Z > 0.f;
	bIsOnGround = Snapshot.bIsOnGround;
	bIsCrouching = Snapshot.bIsCrouching;
}

void UPdAnimInstance::UpdateAcceleration(float DeltaSeconds)
{
	if (bHasPreviousVelocity && DeltaSeconds > UE_SMALL_NUMBER)
	{
		const FVector3f VelocityDelta = Velocity - PreviousVelocity;
		Acceleration = VelocityDelta / DeltaSeconds;
	}
	else
	{
		Acceleration = FVector3f::ZeroVector;
	}

	PreviousVelocity = Velocity;
	bHasPreviousVelocity = true;
}

void UPdAnimInstance::UpdateAimingData(const FGameThreadSnapshot& Snapshot)
{
	AimYaw = Snapshot.AimYaw;
	AimPitch = Snapshot.AimPitch;
}

void UPdAnimInstance::UpdateGrappleState()
{
	check(IsInGameThread());

	const APdPlayer* PlayerCharacter = Cast<APdPlayer>(CachedCharacter.Get());
	bIsGrappling = IsValid(PlayerCharacter) && PlayerCharacter->IsGrappling();
}
