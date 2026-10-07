#include "Animation/AnimNotifyState_LeapToTarget.h"

#include "Components/SkeletalMeshComponent.h"
#include "Curves/CurveFloat.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "Interface/TargetingInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNotifyState_LeapToTarget)

namespace
{
const FName LeapRootMotionName(TEXT("LeapToTarget"));

UCharacterMovementComponent* GetAuthorityMovement(const USkeletalMeshComponent* MeshComp)
{
	const ACharacter* Character = MeshComp ? Cast<ACharacter>(MeshComp->GetOwner()) : nullptr;
	return Character && Character->HasAuthority() ? Character->GetCharacterMovement() : nullptr;
}

/** 두 캡슐 가장자리 사이의 수평 거리. 몬스터 공격 패턴의 거리와 같은 기준이다. */
float GetEdgeDistance2D(const AActor& From, const AActor& To)
{
	float FromRadius = 0.0f;
	float FromHalfHeight = 0.0f;
	From.GetSimpleCollisionCylinder(FromRadius, FromHalfHeight);
	float ToRadius = 0.0f;
	float ToHalfHeight = 0.0f;
	To.GetSimpleCollisionCylinder(ToRadius, ToHalfHeight);
	return FVector::Dist2D(From.GetActorLocation(), To.GetActorLocation()) - FromRadius - ToRadius;
}
}

// 도착 지점은 시작 순간의 대상 위치로 정한다. 도약하는 동안 대상이 피하면 빗나간다.
void UAnimNotifyState_LeapToTarget::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	static_cast<void>(Animation);
	static_cast<void>(EventReference);

	UCharacterMovementComponent* Movement = GetAuthorityMovement(MeshComp);
	AActor* Owner = Movement ? Movement->GetOwner() : nullptr;
	const AActor* Target = Owner && Owner->Implements<UTargetingInterface>() ? ITargetingInterface::Execute_GetAttackTarget(Owner) : nullptr;
	if (!IsValid(Target) || TotalDuration <= 0.0f)
	{
		return;
	}
	FVector Direction = Target->GetActorLocation() - Owner->GetActorLocation();
	Direction.Z = 0.0f;
	const float Distance = FMath::Min(GetEdgeDistance2D(*Owner, *Target) - StopDistance, MaxDistance);
	if (Distance <= 0.0f || !Direction.Normalize())
	{
		return;
	}

	const TSharedPtr<FRootMotionSource_MoveToDynamicForce> Leap = MakeShared<FRootMotionSource_MoveToDynamicForce>();
	Leap->InstanceName = LeapRootMotionName;
	Leap->AccumulateMode = ERootMotionAccumulateMode::Override;
	Leap->Duration = TotalDuration;
	Leap->StartLocation = Owner->GetActorLocation();
	Leap->InitialTargetLocation = Leap->StartLocation + Direction * Distance;
	Leap->TargetLocation = Leap->InitialTargetLocation;
	Leap->TimeMappingCurve = TravelCurve;
	Leap->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::SetVelocity;
	Leap->FinishVelocityParams.SetVelocity = FVector::ZeroVector;
	Movement->ApplyRootMotionSource(Leap);
}

// 공격이 중간에 끊기면 남은 이동도 멈춘다.
void UAnimNotifyState_LeapToTarget::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	static_cast<void>(Animation);
	static_cast<void>(EventReference);

	if (UCharacterMovementComponent* Movement = GetAuthorityMovement(MeshComp))
	{
		Movement->RemoveRootMotionSource(LeapRootMotionName);
	}
}

FString UAnimNotifyState_LeapToTarget::GetNotifyName_Implementation() const
{
	return TEXT("Leap To Target");
}
