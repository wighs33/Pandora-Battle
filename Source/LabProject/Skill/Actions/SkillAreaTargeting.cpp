#include "Skill/Actions/SkillAreaTargeting.h"

#include "Definition/AbilitySystem/SkillAreaSettings.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Map/OutOfBoundsVolume.h"
#include "Map/PlayerMapRegionTrigger.h"
#include "Skill/SkillGroundProjection.h"

namespace
{
	constexpr float AOEGroundProjectionStartHeight = 500.0f;
	constexpr float AOEGroundProjectionMinDepth = 1000.0f;
	constexpr float AOEGroundMinNormalZ = 0.35f;

	bool IsIgnoredAOEGroundActor(const AActor* Actor)
	{
		return Actor && (Actor->IsA<AOutOfBoundsVolume>() || Actor->IsA<APlayerMapRegionTrigger>());
	}

	bool IsValidAOEGroundHit(const FHitResult& Hit)
	{
		return Hit.bBlockingHit && Hit.ImpactNormal.Z >= AOEGroundMinNormalZ && !Cast<APawn>(Hit.GetActor())
			&& !IsIgnoredAOEGroundActor(Hit.GetActor());
	}

	bool TryResolveGroundUnderTarget(AActor& Target, AActor* Avatar, const FSkillAreaSettings& Settings,
		FVector& OutGroundLocation)
	{
		TArray<AActor*> ActorsToIgnore;
		ActorsToIgnore.Add(&Target);
		if (Avatar)
		{
			ActorsToIgnore.Add(Avatar);
		}

		return PdSkillAreaTargeting::TryResolveGroundLocation(Target.GetWorld(), Target.GetActorLocation(),
			ActorsToIgnore, Settings.TargetGroundTraceChannel, static_cast<float>(Settings.TargetGroundTraceDepth),
			OutGroundLocation);
	}
}

bool PdSkillAreaTargeting::TryResolveGroundLocation(UWorld* World, const FVector& SourceLocation,
	const TArray<AActor*>& ActorsToIgnore, const TEnumAsByte<ETraceTypeQuery> TraceType, const float TraceDepth,
	FVector& OutGroundLocation)
{
	if (!World)
	{
		return false;
	}

	const float ResolvedTraceDepth = FMath::Max(TraceDepth, AOEGroundProjectionMinDepth);
	const FVector TraceStart = SourceLocation + FVector::UpVector * AOEGroundProjectionStartHeight;
	const FVector TraceEnd = SourceLocation - FVector::UpVector * ResolvedTraceDepth;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AOEGroundProjection), false);
	for (AActor* ActorToIgnore : ActorsToIgnore)
	{
		if (IsValid(ActorToIgnore))
		{
			QueryParams.AddIgnoredActor(ActorToIgnore);
		}
	}

	// 스킬에 설정된 채널을 먼저 보고, 맞지 않으면 월드 정적·동적 물체로 다시 찾는다.
	const ECollisionChannel TraceChannel = UEngineTypes::ConvertToCollisionChannel(TraceType);
	if (TraceChannel != ECC_MAX)
	{
		TArray<FHitResult> ChannelHits;
		if (World->LineTraceMultiByChannel(ChannelHits, TraceStart, TraceEnd, TraceChannel, QueryParams))
		{
			for (const FHitResult& Hit : ChannelHits)
			{
				if (IsValidAOEGroundHit(Hit))
				{
					OutGroundLocation = Hit.ImpactPoint;
					return true;
				}
			}
		}
	}

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	TArray<FHitResult> ObjectHits;
	if (World->LineTraceMultiByObjectType(ObjectHits, TraceStart, TraceEnd, ObjectParams, QueryParams))
	{
		for (const FHitResult& Hit : ObjectHits)
		{
			if (IsValidAOEGroundHit(Hit))
			{
				OutGroundLocation = Hit.ImpactPoint;
				return true;
			}
		}
	}

	return false;
}

FVector PdSkillAreaTargeting::ResolveTargetLocation(AActor& Target, AActor* Avatar, const FSkillAreaSettings& Settings)
{
	FVector GroundLocation = FVector::ZeroVector;
	return TryResolveGroundUnderTarget(Target, Avatar, Settings, GroundLocation)
		? GroundLocation
		: PdSkillGroundProjection::ResolveActorFeetLocation(&Target);
}

FVector PdSkillAreaTargeting::ResolveForwardLocation(AActor& Avatar, const FSkillAreaSettings& Settings)
{
	const float MaxRange = static_cast<float>(Settings.TargetingMaxRange);
	const float ForwardDistance = FMath::Clamp(MaxRange > 0.0f ? MaxRange * 0.65f : 800.0f, 300.0f, 1200.0f);
	const FVector CandidateLocation = Avatar.GetActorLocation() + Avatar.GetActorForwardVector() * ForwardDistance;

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(&Avatar);
	FVector GroundLocation = FVector::ZeroVector;
	return TryResolveGroundLocation(
		Avatar.GetWorld(),
		CandidateLocation,
		ActorsToIgnore,
		Settings.TargetGroundTraceChannel,
		static_cast<float>(Settings.TargetGroundTraceDepth),
		GroundLocation)
		? GroundLocation
		: CandidateLocation;
}

FVector PdSkillAreaTargeting::ResolveAimedLocation(const FHitResult& HitResult, const FVector& TargetDataEndPoint,
	AActor* Avatar, const FSkillAreaSettings& Settings)
{
	const FVector AimedLocation = HitResult.Location.IsNearlyZero() ? TargetDataEndPoint : HitResult.Location;

	AActor* HitActor = HitResult.GetActor();
	if (IsValid(HitActor) && HitActor->IsA<APawn>())
	{
		return ResolveTargetLocation(*HitActor, Avatar, Settings);
	}

	TArray<AActor*> ActorsToIgnore;
	if (Avatar)
	{
		ActorsToIgnore.Add(Avatar);
	}

	FVector GroundLocation = FVector::ZeroVector;
	UWorld* World = HitActor ? HitActor->GetWorld() : (Avatar ? Avatar->GetWorld() : nullptr);
	return TryResolveGroundLocation(
		World,
		AimedLocation,
		ActorsToIgnore,
		Settings.TargetGroundTraceChannel,
		static_cast<float>(Settings.TargetGroundTraceDepth),
		GroundLocation)
		? GroundLocation
		: AimedLocation;
}
