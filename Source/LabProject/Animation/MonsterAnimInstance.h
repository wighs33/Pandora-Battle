#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "MonsterAnimInstance.generated.h"

/**
 * 몬스터 이동 애님 블루프린트의 부모. 지면 속도에 제자리 회전을 더해 걷기 블렌드에 넘긴다.
 * 회전 전용 애니메이션이 없는 몬스터도 제자리에서 돌 때 다리를 움직이게 하기 위해서다.
 * 보스의 Control Rig(머리 Look-At, 발 IK)에 넘길 대상 위치와 가중치도 여기서 정한다.
 */
UCLASS()
class LABPROJECT_API UMonsterAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	/** 걷기 블렌드스페이스에 넣는 속도. 지면 속도와 제자리 회전을 걷기 속도로 바꾼 값 중 큰 값이다. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "!Monster|Locomotion")
	float LocomotionSpeed = 0.0f;

	/** 이 속도로 돌 때 TurnInPlaceSpeed로 걷는 것처럼 다리를 움직인다. 0이면 회전은 반영하지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "!Monster|Locomotion", meta = (ClampMin = "0.0", ForceUnits = "deg/s"))
	float TurnInPlaceYawRate = 0.0f;

	UPROPERTY(EditDefaultsOnly, Category = "!Monster|Locomotion", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float TurnInPlaceSpeed = 0.0f;

	/** 이동 자세에서 아래턱에 더할 회전(부모 뼈 기준). 원본 애니메이션이 늘 입을 벌리고 있는 몬스터의 입을 닫는다. 공격 몽타주에는 적용하지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Monster|Locomotion")
	FRotator JawCloseRotation = FRotator::ZeroRotator;

	/** 머리가 바라볼 곳(메시 컴포넌트 공간 = Control Rig 전역 공간). 공격 대상의 위치를 부드럽게 따라간다. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "!Monster|ControlRig")
	FVector LookAtLocation = FVector::ZeroVector;

	/** 0~1. 공격 대상이 앞쪽에 있고 몽타주가 없을 때만 올라간다. 공격 동작이 겨누는 방향(브레스, 덮치기)은 바꾸지 않는다. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "!Monster|ControlRig")
	float LookAtWeight = 0.0f;

	/** 0~1. 몽타주가 없을 때만 발을 지면에 맞춘다. 덮치기처럼 발을 드는 동작을 IK가 땅에 붙잡지 않게 한다. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "!Monster|ControlRig")
	float FootIKWeight = 0.0f;

	/** 몸 정면에서 이 각도보다 뒤에 있는 대상은 보지 않는다. 목이 뒤로 꺾이지 않게 한다. */
	UPROPERTY(EditDefaultsOnly, Category = "!Monster|ControlRig", meta = (ClampMin = "0.0", ClampMax = "180.0", ForceUnits = "deg"))
	float LookAtMaxAngle = 110.0f;

	/** 가중치가 0과 1 사이를 오가는 시간. */
	UPROPERTY(EditDefaultsOnly, Category = "!Monster|ControlRig", meta = (ClampMin = "0.01", ForceUnits = "s"))
	float ControlRigBlendTime = 0.3f;

private:
	void UpdateControlRigInputs(const APawn& Pawn, const USkeletalMeshComponent& Mesh, float DeltaSeconds);

	float PreviousMeshYaw = 0.0f;
	bool bHasPreviousMeshYaw = false;
};
