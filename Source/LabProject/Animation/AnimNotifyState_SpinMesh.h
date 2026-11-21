#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_SpinMesh.generated.h"

/**
 * 구간 동안 캐릭터 메시를 제자리에서 돌린다. 몸을 한 바퀴 돌려 꼬리로 둘레를 휩쓰는 공격처럼, 애니메이션에 없는 회전을 덧붙인다.
 *
 * 액터는 돌리지 않고 메시만 돌리므로 이동과 판정 방향은 그대로다. 몽타주가 재생되는 모든 화면이 각자 돌려서 복제가 필요 없다.
 * 원격 화면에서는 이동 보간이 매 프레임 메시 회전을 기준 회전으로 되돌리므로, 기준 회전(CacheInitialMeshOffset)도 함께 바꾼다.
 */
UCLASS(meta = (DisplayName = "Spin Mesh"))
class LABPROJECT_API UAnimNotifyState_SpinMesh : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime,
		const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

protected:
	/** 구간 동안 도는 바퀴 수. 음수면 반대 방향으로 돈다. */
	UPROPERTY(EditAnywhere, Category = "!Spin")
	float Turns = 1.0f;

private:
	/** 메시를 기본 회전에서 Turns 바퀴의 Alpha만큼 돌린다. Alpha가 0이면 기본 회전으로 돌아간다. */
	void ApplySpin(USkeletalMeshComponent& MeshComp, float Alpha) const;
};
