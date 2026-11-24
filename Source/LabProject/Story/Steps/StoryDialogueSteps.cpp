#include "Story/Steps/StoryDialogueSteps.h"

#include "Localization/MenuLocalizationSubsystem.h"
#include "Story/StoryCharacter.h"
#include "Story/StoryCharacterDefinition.h"
#include "Story/StoryDirector.h"
#include "UI/Story/StoryScreen.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StoryDialogueSteps)

namespace
{
	constexpr float LineClipBlend = 0.3f;
	constexpr float ShoutShake = 9.f;
	constexpr float ShoutShakeSeconds = 0.45f;
	/** 눈 뜨기에서 눈꺼풀이 목표를 따라가는 빠르기 */
	constexpr float MashEyeSpeed = 5.f;
	/** 눈꺼풀이 이만큼 따라왔으면 눈을 다 뜬 것으로 본다 */
	constexpr float MashEyesOpen = 0.97f;

	struct FLineTask : FStoryTask
	{
		TWeakObjectPtr<AStoryCharacter> Speaker;

		virtual bool Tick(AStoryDirector& Director, const float DeltaSeconds) override
		{
			UStoryScreen& Screen = Director.GetScreen();
			if (AStoryCharacter* Character = Speaker.Get(); Character && Character->IsTalking() && !Screen.IsTyping())
			{
				Character->SetTalking(false);
			}
			if (!Screen.ConsumeLineAdvance())
			{
				return false;
			}
			if (AStoryCharacter* Character = Speaker.Get())
			{
				Character->SetTalking(false);
			}
			return true;
		}
	};

	struct FMashTask : FStoryTask
	{
		TObjectPtr<const UStoryMashStep> Step;
		float Elapsed = 0.f;
		float Progress = 0.f;
		float Shown = 0.f;

		virtual bool Tick(AStoryDirector& Director, const float DeltaSeconds) override
		{
			UStoryScreen& Screen = Director.GetScreen();
			Elapsed += DeltaSeconds;
			Progress += static_cast<float>(Screen.ConsumeMashPresses()) / Step->Presses;
			Progress = FMath::Clamp(Progress - Step->DecayPerSecond * DeltaSeconds, 0.f, 1.f);
			const bool bTimeUp = Elapsed >= Step->TimeLimit;
			Shown = FMath::FInterpTo(Shown, bTimeUp && Step->bOpensEyes ? 1.f : Progress, DeltaSeconds, MashEyeSpeed);
			Screen.SetMashProgress(Shown);
			if (Step->bOpensEyes)
			{
				Screen.SetEyelids(Shown, 0.f);
			}
			const bool bFilled = Progress >= 1.f && !Step->bAlwaysRunsOut;
			const bool bShownAll = !Step->bOpensEyes || Shown > MashEyesOpen;
			if ((bFilled || bTimeUp) && bShownAll)
			{
				Screen.EndMash();
				return true;
			}
			return false;
		}
	};

	struct FPlayerWalkTask : FStoryTask
	{
		FVector Goal = FVector::ZeroVector;
		float Radius = 100.f;

		virtual bool Tick(AStoryDirector& Director, const float DeltaSeconds) override
		{
			const AStoryCharacter* Player = Director.GetPlayerCharacter();
			if (Player && FVector::Dist2D(Player->GetActorLocation(), Goal) > Radius)
			{
				return false;
			}
			Director.EndPlayerControl();
			return true;
		}
	};
}

TUniquePtr<FStoryTask> UStoryLineStep::Start(AStoryDirector& Director) const
{
	TUniquePtr<FLineTask> Task = MakeUnique<FLineTask>();
	AStoryCharacter* Character = Director.FindCharacter(Speaker);
	if (Character && !bUnseen)
	{
		Director.SetSpeaker(Speaker);
		Character->SetExpression(Expression);
		Character->SetTalking(true);
		if (!Clip.IsNone())
		{
			Character->PlayClip(Clip, true, LineClipBlend);
		}
		Task->Speaker = Character;
	}
	if (bShout)
	{
		Director.Shake(ShoutShake, ShoutShakeSeconds);
	}
	Director.GetScreen().ShowLine(Character ? Character->GetDefinition() : nullptr, TextKey, bShout, Director.GetVoiceSound());
	return Task;
}

TUniquePtr<FStoryTask> UStoryTitleCardStep::Start(AStoryDirector& Director) const
{
	Director.GetScreen().ShowTitleCard(ChapterKey, TitleKey, Seconds);
	return MakeUnique<FStoryWaitTask>(Seconds);
}

TUniquePtr<FStoryTask> UStoryFadeStep::Start(AStoryDirector& Director) const
{
	Director.GetScreen().FadeTo(Color, Alpha, Seconds);
	return MakeUnique<FStoryWaitTask>(Seconds);
}

TUniquePtr<FStoryTask> UStoryEyelidStep::Start(AStoryDirector& Director) const
{
	Director.GetScreen().SetEyelids(Openness, Seconds);
	return MakeUnique<FStoryWaitTask>(Seconds);
}

TUniquePtr<FStoryTask> UStoryMashStep::Start(AStoryDirector& Director) const
{
	Director.GetScreen().BeginMash(PromptKey);
	TUniquePtr<FMashTask> Task = MakeUnique<FMashTask>();
	Task->Step = this;
	return Task;
}

TUniquePtr<FStoryTask> UStoryPlayerWalkStep::Start(AStoryDirector& Director) const
{
	TUniquePtr<FPlayerWalkTask> Task = MakeUnique<FPlayerWalkTask>();
	Task->Goal = Director.GetMark(GoalMark).GetLocation();
	Task->Radius = Radius;
	Director.BeginPlayerControl(Task->Goal, ObjectiveKey);
	return Task;
}
