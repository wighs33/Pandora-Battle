#include "AI/Bot/BotSkillRules.h"

#include "Definition/AbilitySystem/SkillDefinition.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

namespace
{
	/** 스킬 사이에 쉬는 시간의 범위. 매번 조금씩 달라 기계처럼 보이지 않게 한다. */
	constexpr float MinGapBetweenSkillsSeconds = 1.2f;
	constexpr float MaxGapBetweenSkillsSeconds = 2.0f;

	/** 뛰어다니기·제자리 서기를 지키는 최소 시간. */
	constexpr float MinimumRoamOrHoldSeconds = 2.0f;
	/** 기본 공격만 하기를 지키는 최소 시간. */
	constexpr float MinimumWeaponAttacksOnlySeconds = 3.0f;

	/** 대상 둘레를 도는 원: 대상 몸 반지름에 이만큼 더 떨어진다. */
	constexpr float RoamRingGap = 260.0f;
	/** 원에서 벗어난 거리를 이 거리로 나눈 만큼 원 쪽으로 당긴다(최대 1). */
	constexpr float RoamPullDistance = 200.0f;
	/** 도는 방향을 바꾸는 간격의 범위. */
	constexpr float MinRoamTurnSeconds = 1.4f;
	constexpr float MaxRoamTurnSeconds = 2.2f;
	/** 이보다 느리면 막혀 선 것으로 본다. */
	constexpr float RoamStuckSpeed = 60.0f;
	/** 막혀 선 채 이만큼 지나면 도는 방향을 바꾼다. */
	constexpr float RoamStuckSeconds = 0.5f;
}

bool BotSkillRules::IsPandoraUsableByBots(const UPandoraDefinition& Pandora)
{
	const int32 SlotCount = UPandoraDefinition::GetFixedMaxLevel();
	if (Pandora.GetSkillCount() < SlotCount)
	{
		return false;
	}
	for (int32 SkillIndex = 0; SkillIndex < SlotCount; ++SkillIndex)
	{
		const USkillDefinition* Skill = Pandora.GetSkillDefinition(SkillIndex);
		if (!Skill || !Skill->Action)
		{
			return false;
		}
	}
	return true;
}

TArray<const UPandoraDefinition*> BotSkillRules::PickRandomPandoras(TArray<const UPandoraDefinition*> Candidates, const int32 Count)
{
	TArray<const UPandoraDefinition*> Picked;
	while (Picked.Num() < Count && !Candidates.IsEmpty())
	{
		Picked.Add(Candidates[FMath::RandHelper(Candidates.Num())]);
		Candidates.RemoveSingleSwap(Picked.Last());
	}
	return Picked;
}

float BotSkillRules::GetFollowUpSeconds(const USkillDefinition& Skill)
{
	const float Duration = static_cast<float>(Skill.Time.Duration);
	switch (Skill.BotFollowUp)
	{
	case ESkillBotFollowUp::RoamAroundTarget:
	case ESkillBotFollowUp::HoldPosition:
		return FMath::Max(Duration, MinimumRoamOrHoldSeconds);
	case ESkillBotFollowUp::WeaponAttacksOnly:
		return FMath::Max(Duration, MinimumWeaponAttacksOnlySeconds);
	default:
		return 0.0f;
	}
}

float BotSkillRules::RollGapBetweenSkills()
{
	return FMath::FRandRange(MinGapBetweenSkillsSeconds, MaxGapBetweenSkillsSeconds);
}

float BotSkillRules::GetEdgeDistance(const AActor& From, const AActor& To)
{
	float FromRadius = 0.0f;
	float FromHalfHeight = 0.0f;
	From.GetSimpleCollisionCylinder(FromRadius, FromHalfHeight);
	float ToRadius = 0.0f;
	float ToHalfHeight = 0.0f;
	To.GetSimpleCollisionCylinder(ToRadius, ToHalfHeight);
	return FMath::Max(static_cast<float>(FVector::Dist2D(From.GetActorLocation(), To.GetActorLocation())) - FromRadius - ToRadius, 0.0f);
}

bool BotSkillRules::HasLineOfSight(const AActor& From, const AActor& Target)
{
	const UWorld* World = From.GetWorld();
	if (!World)
	{
		return false;
	}
	FVector EyeLocation;
	FRotator EyeRotation;
	From.GetActorEyesViewPoint(EyeLocation, EyeRotation);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BotSkillLineOfSight), false, &From);
	Params.AddIgnoredActor(&Target);
	return !World->LineTraceTestByChannel(EyeLocation, Target.GetActorLocation(), ECC_Visibility, Params);
}

float BotSkillRules::GetYawToTarget(const AActor& From, const AActor& Target)
{
	return (Target.GetActorLocation() - From.GetActorLocation()).Rotation().Yaw;
}

void BotSkillRules::FRoamAroundTarget::Start(const double Now)
{
	CircleSign = FMath::RandBool() ? 1.0f : -1.0f;
	NextTurnTime = Now + FMath::FRandRange(MinRoamTurnSeconds, MaxRoamTurnSeconds);
	StuckSince = -1.0;
}

FVector BotSkillRules::FRoamAroundTarget::Update(const APawn& Self, const AActor& Target, const double Now)
{
	FVector ToTarget = Target.GetActorLocation() - Self.GetActorLocation();
	ToTarget.Z = 0.0f;
	const float Distance = static_cast<float>(ToTarget.Size());
	const FVector Forward = Distance > UE_KINDA_SMALL_NUMBER ? ToTarget / Distance : Self.GetActorForwardVector();
	const float Radius = Target.GetSimpleCollisionRadius() + RoamRingGap;
	const float Pull = FMath::Clamp((Distance - Radius) / RoamPullDistance, -1.0f, 1.0f);

	const bool bStuck = Self.GetVelocity().Size2D() < RoamStuckSpeed;
	StuckSince = bStuck ? (StuckSince < 0.0 ? Now : StuckSince) : -1.0;
	if (Now >= NextTurnTime || (StuckSince >= 0.0 && Now - StuckSince > RoamStuckSeconds))
	{
		CircleSign = -CircleSign;
		NextTurnTime = Now + FMath::FRandRange(MinRoamTurnSeconds, MaxRoamTurnSeconds);
		StuckSince = -1.0;
	}
	return (FVector::CrossProduct(FVector::UpVector, Forward) * CircleSign + Forward * Pull).GetSafeNormal2D();
}
