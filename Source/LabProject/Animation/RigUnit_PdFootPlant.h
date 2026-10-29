#pragma once

#include "CoreMinimal.h"
#include "Units/Highlevel/RigUnit_HighlevelBase.h"
#include "RigUnit_PdFootPlant.generated.h"

/** 지면에 맞출 다리 하나. 허벅지(위팔)부터 발(손)까지 세 뼈다. */
USTRUCT(BlueprintType)
struct FPdFootPlantLeg
{
	GENERATED_BODY()

	/** 허벅지(위팔). */
	UPROPERTY(EditAnywhere, Category = "!Leg", meta = (Input))
	FRigElementKey Upper;

	/** 정강이(아래팔). 무릎(팔꿈치)이 이 뼈의 시작점이다. */
	UPROPERTY(EditAnywhere, Category = "!Leg", meta = (Input))
	FRigElementKey Lower;

	/** 땅에 닿는 발(손). */
	UPROPERTY(EditAnywhere, Category = "!Leg", meta = (Input))
	FRigElementKey Foot;
};

/**
 * 발 IK. 애니메이션은 발밑을 평평한 바닥(리그 공간 Z=0)으로 가정하므로, 발마다 실제 지면 높이를 재서 그 차이만큼 발을 올리거나 내린다.
 * 가장 많이 내려가야 하는 발만큼 골반을 먼저 내려 다리가 닿게 하고, 각 다리는 지금 무릎이 굽은 면을 유지한 채 2본 IK로 푼다.
 * 발의 방향은 애니메이션 그대로 둔다.
 */
USTRUCT(meta = (DisplayName = "Pd Foot Plant", Category = "LabProject|IK", Keywords = "Foot,IK,Ground"))
struct LABPROJECT_API FRigUnit_PdFootPlant : public FRigUnit_HighlevelBaseMutable
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;

	/** 지면에 맞출 다리들. */
	UPROPERTY(meta = (Input))
	TArray<FPdFootPlantLeg> Legs;

	/** 발이 내려가야 할 때 함께 내릴 뼈(골반). 비우면 골반은 그대로 둔다. */
	UPROPERTY(meta = (Input))
	FRigElementKey Pelvis;

	/** 발 위치에서 위아래로 지면을 찾는 거리. */
	UPROPERTY(meta = (Input))
	float TraceHeight = 150.0f;

	/** 발을 올릴 수 있는 최대 높이. */
	UPROPERTY(meta = (Input))
	float MaxRaise = 120.0f;

	/** 발과 골반을 내릴 수 있는 최대 깊이. */
	UPROPERTY(meta = (Input))
	float MaxDrop = 80.0f;

	/** 0이면 애니메이션 그대로, 1이면 지면에 완전히 맞춘다. */
	UPROPERTY(meta = (Input, UIMin = "0.0", UIMax = "1.0"))
	float Weight = 1.0f;
};
