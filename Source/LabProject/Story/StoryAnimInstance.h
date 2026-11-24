#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "StoryAnimInstance.generated.h"

class UAnimSequenceBase;

/** 동작 하나를 재생하고, 동작이 바뀌면 마지막 자세에서 새 동작으로 이어 준다. 머리는 이야기 상대 쪽으로 더 돌린다. */
USTRUCT()
struct FStoryAnimInstanceProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()

public:
	FStoryAnimInstanceProxy() = default;
	explicit FStoryAnimInstanceProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

protected:
	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;

private:
	void ApplyHeadLook(FPoseContext& Output) const;

	TWeakObjectPtr<const UAnimSequenceBase> Clip;
	float Time = 0.f;
	bool bLoop = true;
	float BlendAlpha = 1.f;
	float HeadYaw = 0.f;
	float HeadPitch = 0.f;
	/** 동작이 바뀐 순간의 자세(뼈의 부모 기준 변환). 뼈 수가 달라지면 쓰지 않는다. */
	TArray<FTransform> BlendSource;
	TArray<FTransform> LastPose;
};

/** 이야기 인물의 동작 재생기. 스틱맨 ABP의 발 붙이기·무기 레이어 없이 정해 둔 동작만 재생한다. */
UCLASS(Transient)
class LABPROJECT_API UStoryAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	void Play(const UAnimSequenceBase* InClip, bool bInLoop, float BlendSeconds);
	void SetPlayRate(const float InPlayRate) { ClipPlayRate = InPlayRate; }
	/** 머리를 몸 기준으로 더 돌리는 각도(도). 오른쪽과 위가 +다. */
	void SetHeadLook(const float Yaw, const float Pitch) { HeadYaw = Yaw; HeadPitch = Pitch; }

	const UAnimSequenceBase* GetClip() const { return Clip; }
	/** 되풀이하지 않는 동작이 끝까지 갔는지. */
	bool IsClipFinished() const;

protected:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
	friend struct FStoryAnimInstanceProxy;

	UPROPERTY(Transient)
	TObjectPtr<const UAnimSequenceBase> Clip;

	float Time = 0.f;
	float ClipPlayRate = 1.f;
	bool bLoop = true;
	float BlendLength = 0.f;
	float BlendElapsed = 0.f;
	bool bBlendRequested = false;
	float HeadYaw = 0.f;
	float HeadPitch = 0.f;
};
