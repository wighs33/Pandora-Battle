#include "Animation/PdAnimInstance.h"
#include "Character/CharacterBase.h"
#include "Character/PdPlayer.h"
#include "Weapon/WeaponAnimNotifyNames.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "KismetAnimationLibrary.h"
#include "Component/Player/EquipmentComponent.h"
#include "Weapon/WeaponBase.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(PdAnimInstance)

namespace
{
	/** 전신 몽타주가 쓰는 AnimGraph 슬롯. */
	const FName FullBodySlotName(TEXT("FullBody"));
	/** 이보다 빠르게 움직일 때만 다리에 걷기 모션을 섞는다. 공격 모션의 작은 밀림에는 섞지 않는다. */
	constexpr float LegLocomotionSpeedThreshold = 50.f;
	/** 다리 블렌드가 0↔1을 오가는 초당 변화량. 약 0.15초에 다 바뀐다. */
	constexpr float LegLocomotionBlendRate = 6.5f;
	/** 갱신이 오래 끊겼다가 다시 와도 블렌드가 한 번에 다 바뀌지 않도록, 한 번에 진행하는 시간을 이만큼으로 막는다. */
	constexpr double MaxLegBlendStepSeconds = 0.1;
}

void UPdAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	RefreshGameThreadReferences();
	CaptureGameThreadSnapshot();
	UpdateGrappleState();
}

void UPdAnimInstance::NativeUninitializeAnimation()
{
	check(IsInGameThread());

	CachedCharacter = nullptr;
	MovementComponent = nullptr;
	GameThreadSnapshot = FGameThreadSnapshot();
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

	// NativeUpdateAnimation은 이 단계 전에 끝난다. 값만 담은 스냅샷을 한 번 복사해
	// 워커 경로가 UObject 포인터를 따라가지 않게 한다.
	const FGameThreadSnapshot Snapshot = GameThreadSnapshot;

	if (!Snapshot.bIsValid)
	{
		ResetThreadSafeAnimationData();
		return;
	}

	UpdateLocationData(Snapshot);
	UpdateMovementStates(Snapshot);
	UpdateAimingData(Snapshot);
	UpdateFullBodyLegBlend(Snapshot);
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

void UPdAnimInstance::RefreshGameThreadReferences()
{
	check(IsInGameThread());

	ACharacter* NewCharacter = Cast<ACharacter>(GetOwningActor());
	if (!IsValid(NewCharacter))
	{
		NewCharacter = nullptr;
	}

	UCharacterMovementComponent* NewMovementComponent = NewCharacter ? NewCharacter->GetCharacterMovement() : nullptr;
	if (!IsValid(NewMovementComponent))
	{
		NewMovementComponent = nullptr;
	}

	CachedCharacter = NewCharacter;
	MovementComponent = NewMovementComponent;
}

void UPdAnimInstance::CaptureGameThreadSnapshot()
{
	check(IsInGameThread());

	FGameThreadSnapshot NewSnapshot;

	ACharacter* Character = CachedCharacter.Get();
	UCharacterMovementComponent* CharacterMovement = MovementComponent.Get();
	if (!IsValid(Character) || !IsValid(CharacterMovement))
	{
		GameThreadSnapshot = NewSnapshot;
		return;
	}

	const FVector CurrentVelocity = CharacterMovement->Velocity;
	NewSnapshot.Velocity = FVector3f(CurrentVelocity);
	NewSnapshot.Direction = UKismetAnimationLibrary::CalculateDirection(CurrentVelocity, Character->GetActorRotation());
	NewSnapshot.bIsFalling = CharacterMovement->IsFalling();
	NewSnapshot.bIsOnGround = CharacterMovement->IsMovingOnGround();
	NewSnapshot.bIsCrouching = CharacterMovement->IsCrouching();
	NewSnapshot.bFullBodyMontageActive = GetSlotMontageGlobalWeight(FullBodySlotName) > 0.f;
	NewSnapshot.WorldTimeSeconds = Character->GetWorld()->GetTimeSeconds();

	if (const ACharacterBase* PdCharacter = Cast<ACharacterBase>(Character))
	{
		NewSnapshot.AimYaw = PdCharacter->GetAimYawForAnimation();
		NewSnapshot.AimPitch = PdCharacter->GetAimPitchForAnimation();
	}
	else
	{
		const FRotator AimDelta = (Character->GetBaseAimRotation() - Character->GetActorRotation()).GetNormalized();
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

	bIsMoving = false;
	bIsFalling = false;
	bIsJumping = false;
	bIsCrouching = false;
	bIsOnGround = true;

	AimYaw = 0.f;
	AimPitch = 0.f;
	FullBodyLegLocomotionAlpha = 0.f;
	LastLegBlendWorldTime.Reset();
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

void UPdAnimInstance::UpdateAimingData(const FGameThreadSnapshot& Snapshot)
{
	AimYaw = Snapshot.AimYaw;
	AimPitch = Snapshot.AimPitch;
}

// 블렌드는 월드 시간으로 진행한다. 리슨 서버의 원격 플레이어는 이동 RPC를 받을 때마다 짧게 갱신돼,
// 애니메이션 DeltaSeconds의 합이 실제 시간보다 훨씬 작게 들어오기 때문이다.
void UPdAnimInstance::UpdateFullBodyLegBlend(const FGameThreadSnapshot& Snapshot)
{
	const double Elapsed = LastLegBlendWorldTime.IsSet()
		? FMath::Clamp(Snapshot.WorldTimeSeconds - LastLegBlendWorldTime.GetValue(), 0.0, MaxLegBlendStepSeconds)
		: 0.0;
	LastLegBlendWorldTime = Snapshot.WorldTimeSeconds;

	const bool bWalkingDuringFullBodyMontage = Snapshot.bFullBodyMontageActive && bIsOnGround && GroundSpeed > LegLocomotionSpeedThreshold;
	const float TargetAlpha = bWalkingDuringFullBodyMontage ? 1.f : 0.f;
	FullBodyLegLocomotionAlpha =
		FMath::FInterpConstantTo(FullBodyLegLocomotionAlpha, TargetAlpha, static_cast<float>(Elapsed), LegLocomotionBlendRate);
}

void UPdAnimInstance::UpdateGrappleState()
{
	check(IsInGameThread());

	const APdPlayer* PlayerCharacter = Cast<APdPlayer>(CachedCharacter.Get());
	bIsGrappling = IsValid(PlayerCharacter) && PlayerCharacter->IsGrappling();
}
