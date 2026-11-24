#include "Story/StoryAnimInstance.h"

#include "Animation/AnimationPoseData.h"
#include "Animation/AnimSequenceBase.h"
#include "AnimationRuntime.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StoryAnimInstance)

namespace
{
	const FName NeckBoneName(TEXT("neck_01"));
	const FName HeadBoneName(TEXT("head"));
	/** 목과 머리가 나눠 도는 몫 */
	constexpr float NeckLookShare = 0.35f;
	constexpr float HeadLookShare = 0.65f;

	void RotateBoneInComponentSpace(FCompactPose& Pose, const FName BoneName, const FQuat& Rotation)
	{
		const FBoneContainer& Bones = Pose.GetBoneContainer();
		const int32 MeshBoneIndex = Bones.GetPoseBoneIndexForBoneName(BoneName);
		if (MeshBoneIndex == INDEX_NONE)
		{
			return;
		}
		const FCompactPoseBoneIndex BoneIndex = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshBoneIndex));
		if (!BoneIndex.IsValid())
		{
			return;
		}
		FQuat ParentRotation = FQuat::Identity;
		for (FCompactPoseBoneIndex Parent = Bones.GetParentBoneIndex(BoneIndex); Parent.IsValid(); Parent = Bones.GetParentBoneIndex(Parent))
		{
			ParentRotation = Pose[Parent].GetRotation() * ParentRotation;
		}
		const FQuat LocalRotation = ParentRotation.Inverse() * Rotation * ParentRotation;
		Pose[BoneIndex].SetRotation((LocalRotation * Pose[BoneIndex].GetRotation()).GetNormalized());
	}
}

void FStoryAnimInstanceProxy::PreUpdate(UAnimInstance* InAnimInstance, const float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);

	UStoryAnimInstance* Instance = CastChecked<UStoryAnimInstance>(InAnimInstance);
	if (Instance->bBlendRequested)
	{
		Instance->bBlendRequested = false;
		BlendSource = LastPose;
	}
	Clip = Instance->Clip.Get();
	Time = Instance->Time;
	bLoop = Instance->bLoop;
	BlendAlpha = Instance->BlendLength > 0.f ? FMath::Clamp(Instance->BlendElapsed / Instance->BlendLength, 0.f, 1.f) : 1.f;
	HeadYaw = Instance->HeadYaw;
	HeadPitch = Instance->HeadPitch;
}

bool FStoryAnimInstanceProxy::Evaluate(FPoseContext& Output)
{
	if (const UAnimSequenceBase* Sequence = Clip.Get())
	{
		FAnimationPoseData PoseData(Output);
		Sequence->GetAnimationPose(PoseData, FAnimExtractContext(static_cast<double>(Time), false, {}, bLoop));
	}
	else
	{
		Output.ResetToRefPose();
	}

	const int32 BoneCount = Output.Pose.GetNumBones();
	if (BlendAlpha < 1.f && BlendSource.Num() == BoneCount)
	{
		const float Alpha = FMath::InterpEaseInOut(0.f, 1.f, BlendAlpha, 2.f);
		for (const FCompactPoseBoneIndex BoneIndex : Output.Pose.ForEachBoneIndex())
		{
			FTransform Blended;
			Blended.Blend(BlendSource[BoneIndex.GetInt()], Output.Pose[BoneIndex], Alpha);
			Output.Pose[BoneIndex] = Blended;
		}
	}
	LastPose.SetNumUninitialized(BoneCount);
	for (const FCompactPoseBoneIndex BoneIndex : Output.Pose.ForEachBoneIndex())
	{
		LastPose[BoneIndex.GetInt()] = Output.Pose[BoneIndex];
	}

	ApplyHeadLook(Output);
	return true;
}

// 메시는 +Y를 앞으로 보므로, 머리를 돌리는 축은 컴포넌트의 Z(좌우)와 X(위아래)다.
void FStoryAnimInstanceProxy::ApplyHeadLook(FPoseContext& Output) const
{
	if (FMath::IsNearlyZero(HeadYaw, 0.05f) && FMath::IsNearlyZero(HeadPitch, 0.05f))
	{
		return;
	}
	auto MakeLook = [this](const float Share)
	{
		return FQuat(FVector::UpVector, FMath::DegreesToRadians(HeadYaw * Share)) * FQuat(FVector::XAxisVector, FMath::DegreesToRadians(HeadPitch * Share));
	};
	RotateBoneInComponentSpace(Output.Pose, NeckBoneName, MakeLook(NeckLookShare));
	RotateBoneInComponentSpace(Output.Pose, HeadBoneName, MakeLook(HeadLookShare));
}

FAnimInstanceProxy* UStoryAnimInstance::CreateAnimInstanceProxy()
{
	return new FStoryAnimInstanceProxy(this);
}

void UStoryAnimInstance::Play(const UAnimSequenceBase* InClip, const bool bInLoop, const float BlendSeconds)
{
	if (bInLoop && bLoop && Clip == InClip)
	{
		return;
	}
	Clip = InClip;
	bLoop = bInLoop;
	Time = 0.f;
	ClipPlayRate = 1.f;
	BlendLength = BlendSeconds;
	BlendElapsed = 0.f;
	bBlendRequested = true;
}

bool UStoryAnimInstance::IsClipFinished() const
{
	return !Clip || (!bLoop && Time >= Clip->GetPlayLength());
}

void UStoryAnimInstance::NativeUpdateAnimation(const float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	BlendElapsed += DeltaSeconds;
	if (Clip)
	{
		const float Length = Clip->GetPlayLength();
		Time += DeltaSeconds * ClipPlayRate;
		Time = bLoop && Length > 0.f ? FMath::Fmod(Time, Length) : FMath::Clamp(Time, 0.f, Length);
	}
}
