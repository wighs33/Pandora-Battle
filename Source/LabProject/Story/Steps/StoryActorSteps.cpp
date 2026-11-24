#include "Story/Steps/StoryActorSteps.h"

#include "Story/StoryCharacter.h"
#include "Story/StoryDirector.h"
#include "Story/StoryPortal.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StoryActorSteps)

namespace
{
	/** 빨려 들며 도는 빠르기(도/초)와 마지막 크기 */
	constexpr float PlungeSpin = 900.f;
	constexpr float PlungeEndScale = 0.05f;

	struct FMoveTask : FStoryTask
	{
		TWeakObjectPtr<const AStoryCharacter> Character;

		virtual bool Tick(AStoryDirector& Director, const float DeltaSeconds) override
		{
			return !Character.IsValid() || !Character->IsMoving();
		}
	};

	struct FVanishTask : FStoryTask
	{
		TWeakObjectPtr<AStoryCharacter> Character;
		bool bAppear = false;
		float Seconds = 0.1f;
		float Elapsed = 0.f;

		virtual bool Tick(AStoryDirector& Director, const float DeltaSeconds) override
		{
			AStoryCharacter* Target = Character.Get();
			if (!Target)
			{
				return true;
			}
			Elapsed += DeltaSeconds;
			const float Alpha = FMath::Clamp(Elapsed / Seconds, 0.f, 1.f);
			Target->SetBodyScale(FMath::Max(bAppear ? Alpha : 1.f - Alpha, UE_KINDA_SMALL_NUMBER));
			if (Alpha < 1.f)
			{
				return false;
			}
			Target->SetActorHiddenInGame(!bAppear);
			return true;
		}
	};

	struct FPlungeTask : FStoryTask
	{
		TWeakObjectPtr<AStoryCharacter> Character;
		FVector Start = FVector::ZeroVector;
		FVector End = FVector::ZeroVector;
		float Seconds = 1.f;
		float Elapsed = 0.f;

		virtual bool Tick(AStoryDirector& Director, const float DeltaSeconds) override
		{
			AStoryCharacter* Target = Character.Get();
			if (!Target)
			{
				return true;
			}
			Elapsed += DeltaSeconds;
			const float Alpha = FMath::Clamp(Elapsed / Seconds, 0.f, 1.f);
			const float Eased = FMath::InterpEaseIn(0.f, 1.f, Alpha, 2.f);
			Target->SetActorLocation(FMath::Lerp(Start, End, Eased));
			Target->AddMeshSpin(PlungeSpin * Alpha * DeltaSeconds);
			Target->SetBodyScale(FMath::Lerp(1.f, PlungeEndScale, Eased));
			if (Alpha < 1.f)
			{
				return false;
			}
			Target->SetActorHiddenInGame(true);
			if (AStoryPortal* Portal = Director.GetPortal())
			{
				Portal->Flash();
			}
			return true;
		}
	};
}

TUniquePtr<FStoryTask> UStoryPerformStep::Start(AStoryDirector& Director) const
{
	AStoryCharacter* Target = Director.FindCharacter(Character);
	if (!Target)
	{
		return nullptr;
	}
	Target->SetHeadLookEnabled(bHeadLook);
	Target->SetExpression(Expression);
	Target->PlayClip(Clip, bLoop, BlendSeconds);
	const float Seconds = bLoop || Clip.IsNone() ? HoldSeconds : Target->GetClipLength(Clip);
	return Seconds > 0.f ? MakeUnique<FStoryWaitTask>(Seconds) : nullptr;
}

TUniquePtr<FStoryTask> UStoryMoveStep::Start(AStoryDirector& Director) const
{
	AStoryCharacter* Target = Director.FindCharacter(Character);
	if (!Target)
	{
		return nullptr;
	}
	const FRotator Facing(0.f, Target->GetActorRotation().Yaw, 0.f);
	const FTransform Destination = Mark.IsNone()
		? FTransform(Facing, Target->GetGroundLocation() + Facing.RotateVector(Offset))
		: Director.GetMark(Mark);
	if (bTeleport)
	{
		Target->PlaceAt(Destination.GetLocation(), Destination.Rotator().Yaw);
		return nullptr;
	}
	Target->MoveTo(Destination.GetLocation(), Speed, bKeepFacing);
	TUniquePtr<FMoveTask> Task = MakeUnique<FMoveTask>();
	Task->Character = Target;
	return Task;
}

TUniquePtr<FStoryTask> UStoryFaceStep::Start(AStoryDirector& Director) const
{
	AStoryCharacter* Target = Director.FindCharacter(Character);
	if (!Target)
	{
		return nullptr;
	}
	if (const AStoryCharacter* Other = TargetCharacter.IsNone() ? nullptr : Director.FindCharacter(TargetCharacter))
	{
		Target->FaceLocation(Other->GetActorLocation(), false);
	}
	else
	{
		Target->FaceLocation(Director.GetMark(TargetMark).GetLocation(), false);
	}
	return HoldSeconds > 0.f ? MakeUnique<FStoryWaitTask>(HoldSeconds) : nullptr;
}

TUniquePtr<FStoryTask> UStoryEmoteStep::Start(AStoryDirector& Director) const
{
	for (const FName Id : Characters)
	{
		if (AStoryCharacter* Target = Director.FindCharacter(Id))
		{
			Target->ShowEmote(Emote, Seconds);
		}
	}
	return MakeUnique<FStoryWaitTask>(Seconds);
}

TUniquePtr<FStoryTask> UStoryGlowStep::Start(AStoryDirector& Director) const
{
	if (AStoryCharacter* Target = Director.FindCharacter(Character))
	{
		Target->SetGlow(Intensity);
	}
	return nullptr;
}

TUniquePtr<FStoryTask> UStoryWanderStep::Start(AStoryDirector& Director) const
{
	AStoryCharacter* Target = Director.FindCharacter(Character);
	if (!Target)
	{
		return nullptr;
	}
	if (!bWander)
	{
		Target->StopWander();
		return nullptr;
	}
	FStoryWander Wander;
	Wander.Center = Director.FindCharacter(Center);
	for (const FName Id : Avoid)
	{
		Wander.Avoid.Add(Director.FindCharacter(Id));
	}
	Wander.FacingYaw = FacingYaw;
	Wander.HalfArc = HalfArc;
	Wander.NearRadius = NearRadius;
	Wander.FarRadius = FarRadius;
	Wander.Speed = Speed;
	Target->StartWander(Wander);
	return nullptr;
}

TUniquePtr<FStoryTask> UStoryVanishStep::Start(AStoryDirector& Director) const
{
	AStoryCharacter* Target = Director.FindCharacter(Character);
	if (!Target)
	{
		return nullptr;
	}
	if (bAppear)
	{
		Target->SetBodyScale(UE_KINDA_SMALL_NUMBER);
		Target->SetActorHiddenInGame(false);
	}
	TUniquePtr<FVanishTask> Task = MakeUnique<FVanishTask>();
	Task->Character = Target;
	Task->bAppear = bAppear;
	Task->Seconds = Seconds;
	return Task;
}

TUniquePtr<FStoryTask> UStoryPlungeStep::Start(AStoryDirector& Director) const
{
	AStoryCharacter* Target = Director.FindCharacter(Character);
	if (!Target)
	{
		return nullptr;
	}
	TUniquePtr<FPlungeTask> Task = MakeUnique<FPlungeTask>();
	Task->Character = Target;
	Task->Start = Target->GetActorLocation();
	Task->End = Director.GetMark(Mark).GetLocation();
	Task->Seconds = Seconds;
	return Task;
}
