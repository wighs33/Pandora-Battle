#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "MonsterAnimInstance.generated.h"

/**
 * 몬스터 이동 애님 블루프린트의 부모. 지면 속도에 제자리 회전을 더해 걷기 블렌드에 넘긴다.
 * 회전 전용 애니메이션이 없는 몬스터도 제자리에서 돌 때 다리를 움직이게 하기 위해서다.
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

private:

	float PreviousMeshYaw = 0.0f;
	bool bHasPreviousMeshYaw = false;
};
