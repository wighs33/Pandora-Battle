#include "Animation/MonsterAnimInstance.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"

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
}
