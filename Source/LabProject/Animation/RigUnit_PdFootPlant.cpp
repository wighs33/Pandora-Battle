#include "Animation/RigUnit_PdFootPlant.h"

#include "Engine/World.h"
#include "TwoBoneIK.h"
#include "Units/RigUnitContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_PdFootPlant)

FRigUnit_PdFootPlant_Execute()
{
	URigHierarchy* Hierarchy = ExecuteContext.Hierarchy;
	const UWorld* World = ExecuteContext.GetWorld();
	if (!Hierarchy || !World || Weight <= UE_SMALL_NUMBER || Legs.IsEmpty())
	{
		return;
	}

	// 1) 발마다 지면 높이(애니메이션이 가정한 바닥 Z=0과의 차이)를 잰다.
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PdFootPlant), false, ExecuteContext.GetOwningActor());
	TArray<FVector, TInlineAllocator<4>> FootTargets;
	float LowestOffset = 0.0f;
	for (const FFootPlantLeg& Leg : Legs)
	{
		const FVector Foot = Hierarchy->GetGlobalTransform(Leg.Foot).GetLocation();
		FHitResult Hit;
		float Offset = 0.0f;
		if (World->LineTraceSingleByChannel(Hit, ExecuteContext.ToWorldSpace(FVector(Foot.X, Foot.Y, TraceHeight)),
			ExecuteContext.ToWorldSpace(FVector(Foot.X, Foot.Y, -TraceHeight)), ECC_Visibility, QueryParams))
		{
			Offset = FMath::Clamp(ExecuteContext.ToVMSpace(Hit.ImpactPoint).Z, -MaxDrop, MaxRaise) * Weight;
		}
		FootTargets.Add(Foot + FVector(0.0f, 0.0f, Offset));
		LowestOffset = FMath::Min(LowestOffset, Offset);
	}

	// 2) 가장 낮은 발만큼 골반을 내린다. 다리는 골반을 따라 함께 내려간다.
	if (LowestOffset < 0.0f && Pelvis.IsValid())
	{
		FTransform PelvisTransform = Hierarchy->GetGlobalTransform(Pelvis);
		PelvisTransform.AddToTranslation(FVector(0.0f, 0.0f, LowestOffset));
		Hierarchy->SetGlobalTransform(Pelvis, PelvisTransform, false, true);
	}

	// 3) 다리마다 2본 IK. 무릎 방향은 지금 자세에서 무릎이 엉덩이-발 직선에서 벗어난 쪽으로 둔다.
	for (int32 Index = 0; Index < Legs.Num(); ++Index)
	{
		const FFootPlantLeg& Leg = Legs[Index];
		FTransform Upper = Hierarchy->GetGlobalTransform(Leg.Upper);
		FTransform Lower = Hierarchy->GetGlobalTransform(Leg.Lower);
		FTransform Foot = Hierarchy->GetGlobalTransform(Leg.Foot);
		if (FVector::DistSquared(Foot.GetLocation(), FootTargets[Index]) < 0.25f)
		{
			continue;
		}
		const FQuat FootRotation = Foot.GetRotation();
		const FVector Bend = Lower.GetLocation() - (Upper.GetLocation() + Foot.GetLocation()) * 0.5f;
		const FVector KneeTarget = Lower.GetLocation() + Bend.GetSafeNormal() * FVector::Dist(Upper.GetLocation(), Lower.GetLocation());
		AnimationCore::SolveTwoBoneIK(Upper, Lower, Foot, KneeTarget, FootTargets[Index], false, 1.0, 1.0);
		Foot.SetRotation(FootRotation);
		// 세 번째 인자(bInitial)는 false여야 지금 포즈가 바뀐다. 허벅지와 정강이는 자식을 두고 옮기고, 발은 발가락과 함께 옮긴다.
		Hierarchy->SetGlobalTransform(Leg.Upper, Upper, false, false);
		Hierarchy->SetGlobalTransform(Leg.Lower, Lower, false, false);
		Hierarchy->SetGlobalTransform(Leg.Foot, Foot, false, true);
	}
}
