#include "Animation/PdAnimInstance.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "KismetAnimationLibrary.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(PdAnimInstance)

DEFINE_LOG_CATEGORY(CommonAnimInstanceLog);

void UPdAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	CachedCharacter = Cast<ACharacter>(GetOwningActor());
	if (!CachedCharacter)
	{
		return;
	}

	MovementComponent = CachedCharacter->GetCharacterMovement();
}

void UPdAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

	// 주의: 워커 스레드에서 실행되므로 외부 오브젝트의 값을 수정하면 안됩니다
	// 읽기만 허용되며, AnimInstance 내부 변수에만 값을 저장해야 합니다

	if (!MovementComponent)
	{
		return;
	}

	UpdateLocationData(DeltaSeconds);
	UpdateMovementStates();
	UpdateAcceleration(DeltaSeconds);
	UpdateAimingData();
}

void UPdAnimInstance::UpdateLocationData(float DeltaSeconds)
{
	// 속도
	Velocity = FVector3f(MovementComponent->Velocity);
	GroundSpeed = Velocity.Size2D();
	VerticalVelocity = Velocity.Z;

	// 방향 (-180 ~ 180)
	Direction = UKismetAnimationLibrary::CalculateDirection(
		MovementComponent->Velocity,
		CachedCharacter->GetActorRotation()
	);
}

void UPdAnimInstance::UpdateMovementStates()
{
	// 이동 중 판정
	bIsMoving = GroundSpeed > MovingSpeedThreshold;

	// 낙하/점프 상태
	bIsFalling = MovementComponent->IsFalling();
	bIsJumping = bIsFalling && Velocity.Z > 0.f;
	bIsOnGround = MovementComponent->IsMovingOnGround();

	// 웅크리기
	bIsCrouching = MovementComponent->IsCrouching();
}

void UPdAnimInstance::UpdateAcceleration(float DeltaSeconds)
{
	if (DeltaSeconds > UE_SMALL_NUMBER)
	{
		const FVector3f VelocityDelta = Velocity - PreviousVelocity;
		Acceleration = VelocityDelta / DeltaSeconds;
	}

	PreviousVelocity = Velocity;
}

void UPdAnimInstance::UpdateAimingData()
{
	// Control Rotation: 카메라/마우스가 바라보는 방향
	// Actor Rotation: 캐릭터 몸이 향하는 방향
	const FRotator ControlRotation = CachedCharacter->GetControlRotation();
	const FRotator ActorRotation = CachedCharacter->GetActorRotation();

	// 차이 계산 후 정규화 (-180 ~ 180)
	const FRotator Delta = (ControlRotation - ActorRotation).GetNormalized();

	AimYaw = Delta.Yaw;
	AimPitch = Delta.Pitch;
}