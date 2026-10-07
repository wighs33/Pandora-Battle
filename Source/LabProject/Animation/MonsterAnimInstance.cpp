#include "Animation/MonsterAnimInstance.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Interface/TargetingInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MonsterAnimInstance)

// 메시 방향은 원격 화면의 네트워크 보정까지 반영돼 부드럽게 바뀌므로, 메시 회전으로 회전 속도를 잰다.
void UMonsterAnimInstance::NativeUpdateAnimation(const float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	const APawn* Pawn = TryGetPawnOwner();
	const USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	if (!Pawn || !Mesh || DeltaSeconds <= 0.0f)
	{
		LocomotionSpeed = 0.0f;
		return;
	}
	const float MeshYaw = Mesh->GetComponentRotation().Yaw;
	const float YawRate = bHasPreviousMeshYaw ? FMath::Abs(FRotator::NormalizeAxis(MeshYaw - PreviousMeshYaw)) / DeltaSeconds : 0.0f;
	PreviousMeshYaw = MeshYaw;
	bHasPreviousMeshYaw = true;
	const float TurnSpeed = TurnInPlaceYawRate > 0.0f ? YawRate / TurnInPlaceYawRate * TurnInPlaceSpeed : 0.0f;
	LocomotionSpeed = FMath::Max(Pawn->GetVelocity().Size2D(), TurnSpeed);
	UpdateControlRigInputs(*Pawn, *Mesh, DeltaSeconds);
}

// 공격 대상은 서버가 고르고 몬스터가 복제하므로 모든 화면에서 같은 쪽을 본다(머리에 붙은 공격 판정도 서버와 같다).
void UMonsterAnimInstance::UpdateControlRigInputs(const APawn& Pawn, const USkeletalMeshComponent& Mesh, const float DeltaSeconds)
{
	const AActor* Target = Pawn.Implements<UTargetingInterface>() ? ITargetingInterface::Execute_GetAttackTarget(&Pawn) : nullptr;
	const bool bMontagePlaying = IsAnyMontagePlaying();
	bool bLookAtTarget = false;
	if (Target)
	{
		const FVector ToTarget = (Target->GetActorLocation() - Pawn.GetActorLocation()).GetSafeNormal2D();
		bLookAtTarget = !bMontagePlaying
			&& FVector::DotProduct(ToTarget, Pawn.GetActorForwardVector().GetSafeNormal2D()) >= FMath::Cos(FMath::DegreesToRadians(LookAtMaxAngle));
		const FVector TargetLocation = Mesh.GetComponentTransform().InverseTransformPosition(Target->GetActorLocation());
		LookAtLocation = LookAtWeight > 0.0f ? FMath::VInterpTo(LookAtLocation, TargetLocation, DeltaSeconds, 6.0f) : TargetLocation;
	}
	const float BlendSpeed = 1.0f / ControlRigBlendTime;
	LookAtWeight = FMath::FInterpConstantTo(LookAtWeight, bLookAtTarget ? 1.0f : 0.0f, DeltaSeconds, BlendSpeed);
	FootIKWeight = FMath::FInterpConstantTo(FootIKWeight, bMontagePlaying ? 0.0f : 1.0f, DeltaSeconds, BlendSpeed);
}
