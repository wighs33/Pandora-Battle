#include "Component/Player/UnarmedAttackSweep.h"

#include "Character/CharacterBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Definition/Common/CombatSettings.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Kismet/KismetSystemLibrary.h"

namespace
{
	constexpr float UnarmedTraceDebugDrawTime = 1.0f;
	const FColor UnarmedTraceDebugColor = FColor::Red;
	const FColor UnarmedTraceDebugHitColor = FColor::Green;
}

bool FUnarmedAttackSweep::CanSweep(const FUnarmedCombatSettings& Settings)
{
	return !Settings.AttackTraces.IsEmpty() && !Settings.TraceObjectTypes.IsEmpty()
		&& FMath::IsFinite(Settings.TraceInterval) && Settings.TraceInterval > 0.0f
		&& FMath::IsFinite(Settings.TraceInterpolationDistance) && Settings.TraceInterpolationDistance > 0.0f
		&& FMath::IsFinite(Settings.MaxTraceTravelDistance) && Settings.MaxTraceTravelDistance > 0.0f;
}

void FUnarmedAttackSweep::EnterSection(const FName AttackSectionName, const int32 TraceCount)
{
	if (TrackedSectionName != AttackSectionName)
	{
		TrackedSectionName = AttackSectionName;
		HitActorsInSection.Reset();
		PreviousTraceValid.Init(0, TraceCount);
	}
}

void FUnarmedAttackSweep::ResetHitTracking(const int32 TraceCount)
{
	++Generation;
	PreviousTraceValid.Init(0, TraceCount);
	TrackedSectionName = NAME_None;
	HitActorsInSection.Reset();
}

void FUnarmedAttackSweep::Begin(const int32 TraceCount)
{
	bActive = true;
	++Generation;
	ResetPreviousTraces(TraceCount);
}

void FUnarmedAttackSweep::End()
{
	bActive = false;
	++Generation;
	PreviousTraceStartLocations.Reset();
	PreviousTraceEndLocations.Reset();
	PreviousTraceValid.Reset();
}

void FUnarmedAttackSweep::ResetPreviousTraces(const int32 TraceCount)
{
	PreviousTraceStartLocations.SetNumZeroed(TraceCount);
	PreviousTraceEndLocations.SetNumZeroed(TraceCount);
	PreviousTraceValid.Init(0, TraceCount);
}

void FUnarmedAttackSweep::Sweep(
	const UObject& WorldContext,
	const FUnarmedCombatSettings& Settings,
	ACharacterBase& SourceCharacter,
	USkeletalMeshComponent& SourceMesh,
	const bool bDrawDebug,
	const TFunctionRef<void(AActor*)> OnNewHit)
{
	UWorld* World = WorldContext.GetWorld();
	if (!bActive || !World || Settings.AttackTraces.IsEmpty() || Settings.TraceObjectTypes.IsEmpty())
	{
		return;
	}

	const uint32 SweepGeneration = Generation;
	ActorsToIgnore.Reset(1);
	ActorsToIgnore.Add(&SourceCharacter);

	const int32 TraceCount = Settings.AttackTraces.Num();
	if (PreviousTraceStartLocations.Num() != TraceCount
		|| PreviousTraceEndLocations.Num() != TraceCount
		|| PreviousTraceValid.Num() != TraceCount)
	{
		ResetPreviousTraces(TraceCount);
	}

	for (int32 TraceIndex = 0; TraceIndex < TraceCount; ++TraceIndex)
	{
		const FUnarmedAttackTraceDefinition& TraceDefinition = Settings.AttackTraces[TraceIndex];
		if (TraceDefinition.StartSocketName.IsNone() || !SourceMesh.DoesSocketExist(TraceDefinition.StartSocketName))
		{
			continue;
		}

		const FName EndSocketName = TraceDefinition.EndSocketName.IsNone()
			? TraceDefinition.StartSocketName
			: TraceDefinition.EndSocketName;
		if (!SourceMesh.DoesSocketExist(EndSocketName))
		{
			continue;
		}

		const FVector TraceStart = SourceMesh.GetSocketLocation(TraceDefinition.StartSocketName);
		FVector TraceEnd = SourceMesh.GetSocketLocation(EndSocketName);
		if (TraceStart.Equals(TraceEnd, KINDA_SMALL_NUMBER))
		{
			TraceEnd = TraceStart + SourceCharacter.GetActorForwardVector();
		}

		HitResults.Reset();
		const FVector TraceHalfSize = TraceDefinition.HalfSize;
		if (TraceHalfSize.ContainsNaN()
			|| TraceHalfSize.X <= 0.0f
			|| TraceHalfSize.Y <= 0.0f
			|| TraceHalfSize.Z <= 0.0f)
		{
			continue;
		}
		const FRotator TraceRotation = SourceCharacter.GetActorRotation();
		if (TraceStart.ContainsNaN() || TraceEnd.ContainsNaN())
		{
			PreviousTraceValid[TraceIndex] = 0;
			continue;
		}
		bool bHasPreviousTrace = PreviousTraceValid[TraceIndex] != 0;
		if (bHasPreviousTrace)
		{
			const double TravelDistance = FMath::Max(
				FVector::Distance(PreviousTraceStartLocations[TraceIndex], TraceStart),
				FVector::Distance(PreviousTraceEndLocations[TraceIndex], TraceEnd));
			// 순간이동이나 큰 위치 보정은 이전 위치에서 이어서 휘두른 공격으로 취급하지 않는다.
			bHasPreviousTrace = FMath::IsFinite(TravelDistance) && TravelDistance <= Settings.MaxTraceTravelDistance;
		}
		const FVector PreviousTraceStart = bHasPreviousTrace
			? PreviousTraceStartLocations[TraceIndex]
			: TraceStart;
		const FVector PreviousTraceEnd = bHasPreviousTrace
			? PreviousTraceEndLocations[TraceIndex]
			: TraceEnd;
		const float MaxTravelDistance = FMath::Max(
			FVector::Distance(PreviousTraceStart, TraceStart),
			FVector::Distance(PreviousTraceEnd, TraceEnd));
		const float InterpolationDistance = Settings.TraceInterpolationDistance;
		const int32 MaxSteps = FMath::Clamp(Settings.MaxTraceInterpolationSteps, 1, 64);
		const int32 InterpolationCount = FMath::CeilToInt(FMath::Clamp(MaxTravelDistance / InterpolationDistance, 1.0f, static_cast<float>(MaxSteps)));

		for (int32 InterpolationIndex = 1; InterpolationIndex <= InterpolationCount; ++InterpolationIndex)
		{
			const float Alpha =
				static_cast<float>(InterpolationIndex) / static_cast<float>(InterpolationCount);
			const FVector InterpolatedTraceStart = FMath::Lerp(PreviousTraceStart, TraceStart, Alpha);
			const FVector InterpolatedTraceEnd = FMath::Lerp(PreviousTraceEnd, TraceEnd, Alpha);
			InterpolatedHitResults.Reset();
			UKismetSystemLibrary::BoxTraceMultiForObjects(
				&WorldContext,
				InterpolatedTraceStart,
				InterpolatedTraceEnd,
				TraceHalfSize,
				TraceRotation,
				Settings.TraceObjectTypes,
				false,
				ActorsToIgnore,
				EDrawDebugTrace::None,
				InterpolatedHitResults,
				true,
				FLinearColor::Red,
				FLinearColor::Green,
				UnarmedTraceDebugDrawTime);
			HitResults.Append(InterpolatedHitResults);
		}

		PreviousTraceStartLocations[TraceIndex] = TraceStart;
		PreviousTraceEndLocations[TraceIndex] = TraceEnd;
		PreviousTraceValid[TraceIndex] = 1;

		if (bDrawDebug)
		{
			const bool bAnyHit = HitResults.ContainsByPredicate(
				[this](const FHitResult& Hit)
				{
					return Hit.GetActor() && !HitActorsInSection.Contains(Hit.GetActor());
				});
			const FColor DrawColor = bAnyHit
				? UnarmedTraceDebugHitColor
				: UnarmedTraceDebugColor;
			DrawDebugBox(World, TraceStart, TraceHalfSize, TraceRotation.Quaternion(), DrawColor, false, UnarmedTraceDebugDrawTime, 0, 1.5f);
			DrawDebugLine(World, TraceStart, TraceEnd, DrawColor, false, UnarmedTraceDebugDrawTime, 0, 2.0f);
		}

		for (const FHitResult& HitResult : HitResults)
		{
			AActor* HitActor = HitResult.GetActor();
			if (!HitActor || HitActorsInSection.Contains(HitActor))
			{
				continue;
			}

			ACharacterBase* HitCharacter = Cast<ACharacterBase>(HitActor);
			if (!HitCharacter || HitCharacter == &SourceCharacter)
			{
				continue;
			}

			if (!SourceCharacter.CanDamageCharacterByTeam(HitCharacter))
			{
				continue;
			}

			HitActorsInSection.Add(HitActor);
			OnNewHit(HitActor);
			if (SweepGeneration != Generation)
			{
				return;
			}
		}
	}
}
