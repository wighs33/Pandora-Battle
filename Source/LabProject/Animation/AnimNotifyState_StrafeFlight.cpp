#include "Animation/AnimNotifyState_StrafeFlight.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNotifyState_StrafeFlight)

namespace
{
const FName StrafeRootMotionName(TEXT("StrafeFlight"));

/** 날개 원기둥을 살피는 방향 수. 경기장 벽의 기둥 사이로 선이 빠져나가지 않을 만큼 촘촘히 둔다. */
constexpr int32 WingSpaceDirections = 48;
/** 날개 원기둥을 살피는 높이 간격. */
constexpr float WingSpaceLayerSpacing = 500.0f;
/** 바닥을 벽으로 보지 않게 발에서 이만큼 띄운 높이부터 살핀다. */
constexpr float WingSpaceFloorOffset = 150.0f;
/** 비행 길을 살피는 간격. 처음 막히는 자리 바로 앞 살핀 자리까지만 간다. */
constexpr float FlightPathSampleSpacing = 200.0f;

ACharacter* GetCharacter(const USkeletalMeshComponent* MeshComp)
{
	return MeshComp ? Cast<ACharacter>(MeshComp->GetOwner()) : nullptr;
}
}

// 시작할 때 갈 거리를 한 번 정해 곧게 날아간다. 갈 자리가 없으면 제자리에서 날갯짓만 한다.
void UAnimNotifyState_StrafeFlight::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	ACharacter* Character = GetCharacter(MeshComp);
	if (!Character)
	{
		return;
	}
	SetCapsuleBlocksPawns(*Character, false);

	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	if (!Character->HasAuthority() || !Movement || TotalDuration <= 0.0f)
	{
		return;
	}
	const float Distance = FindClearDistance(*Character, FlightSpeed * TotalDuration);
	if (Distance <= 0.0f)
	{
		return;
	}
	const TSharedPtr<FRootMotionSource_MoveToForce> Flight = MakeShared<FRootMotionSource_MoveToForce>();
	Flight->InstanceName = StrafeRootMotionName;
	Flight->AccumulateMode = ERootMotionAccumulateMode::Override;
	Flight->Duration = TotalDuration;
	Flight->StartLocation = Character->GetActorLocation();
	Flight->TargetLocation = Flight->StartLocation + Character->GetActorForwardVector().GetSafeNormal2D() * Distance;
	Flight->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::SetVelocity;
	Flight->FinishVelocityParams.SetVelocity = FVector::ZeroVector;
	Movement->ApplyRootMotionSource(Flight);
}

// 공격이 중간에 끊겨도 비행과 충돌 설정을 함께 되돌린다.
void UAnimNotifyState_StrafeFlight::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	if (ACharacter* Character = GetCharacter(MeshComp))
	{
		SetCapsuleBlocksPawns(*Character, true);
		if (UCharacterMovementComponent* Movement = Character->HasAuthority() ? Character->GetCharacterMovement() : nullptr)
		{
			Movement->RemoveRootMotionSource(StrafeRootMotionName);
		}
	}
	Super::NotifyEnd(MeshComp, Animation, EventReference);
}

FString UAnimNotifyState_StrafeFlight::GetNotifyName_Implementation() const
{
	return TEXT("Strafe Flight");
}

bool UAnimNotifyState_StrafeFlight::HasRoomToFly(const ACharacter& Character) const
{
	return IsWingSpaceClear(Character, GetFeetLocation(Character), Character.GetActorForwardVector().GetSafeNormal2D(), false);
}

bool UAnimNotifyState_StrafeFlight::IsWingSpaceClear(const ACharacter& Character, const FVector& Feet, const FVector& Forward,
	const bool bFrontOnly) const
{
	const UWorld* World = Character.GetWorld();
	if (!World)
	{
		return false;
	}
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(StrafeFlightWingSpace), false, &Character);
	for (float Height = WingSpaceFloorOffset; Height <= WingTop; Height += WingSpaceLayerSpacing)
	{
		const FVector Axis = Feet + FVector(0.0f, 0.0f, Height);
		for (int32 Index = 0; Index < WingSpaceDirections; ++Index)
		{
			const FVector Direction = FRotator(0.0f, 360.0f * Index / WingSpaceDirections, 0.0f).Vector();
			if (bFrontOnly && FVector::DotProduct(Direction, Forward) < 0.0f)
			{
				continue;
			}
			if (World->LineTraceTestByObjectType(Axis, Axis + Direction * WingReach, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
			{
				return false;
			}
		}
	}
	return true;
}

float UAnimNotifyState_StrafeFlight::FindClearDistance(const ACharacter& Character, const float MaxDistance) const
{
	const FVector Feet = GetFeetLocation(Character);
	const FVector Forward = Character.GetActorForwardVector().GetSafeNormal2D();
	float ClearDistance = 0.0f;
	while (ClearDistance + FlightPathSampleSpacing <= MaxDistance
		&& IsWingSpaceClear(Character, Feet + Forward * (ClearDistance + FlightPathSampleSpacing), Forward, true))
	{
		ClearDistance += FlightPathSampleSpacing;
	}
	return ClearDistance;
}

FVector UAnimNotifyState_StrafeFlight::GetFeetLocation(const ACharacter& Character)
{
	const UCapsuleComponent* Capsule = Character.GetCapsuleComponent();
	return Character.GetActorLocation() - FVector(0.0f, 0.0f, Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 0.0f);
}

void UAnimNotifyState_StrafeFlight::SetCapsuleBlocksPawns(ACharacter& Character, const bool bBlocks)
{
	UCapsuleComponent* Capsule = Character.GetCapsuleComponent();
	const ACharacter* Defaults = Character.GetClass()->GetDefaultObject<ACharacter>();
	const UCapsuleComponent* DefaultCapsule = Defaults ? Defaults->GetCapsuleComponent() : nullptr;
	if (Capsule && DefaultCapsule)
	{
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, bBlocks ? DefaultCapsule->GetCollisionResponseToChannel(ECC_Pawn) : ECR_Ignore);
	}
}
