#include "Skill/Actions/SkillAction.h"
#include "AbilitySystem/Ability/SkillAbility.h"

void USkillAction::Start(USkillAbility* InAbility, const FSkillActionContext& InContext)
{
	check(!bRunning);
	OwningAbility = InAbility;
	ExecutionContext = InContext;
	bRunning = true;
	// 타이머가 처리되기 전이라도 공통 종료 시점 이후에는 다음 액션을 실행하지 않는다.
	if (InAbility && InAbility->HasDurationDeadline() && InAbility->GetRemainingDuration() <= 0.0f)
	{
		Finish();
		return;
	}
	OnStart();
}

UWorld* USkillAction::GetWorld() const
{
	return OwningAbility.IsValid() ? OwningAbility->GetWorld() : nullptr;
}

void USkillAction::Cancel()
{
	Finish(false);
}

void USkillAction::Finish(bool bSucceeded)
{
	if (!bRunning) return;
	bRunning = false;
	OnStop();
	OnFinished.Broadcast(this, bSucceeded);
	OnFinished.Clear();
}

void USkillSequenceAction::OnStart()
{
	NextIndex = 0;
	StartNext();
}

void USkillSequenceAction::StartNext()
{
	if (!IsRunning()) return;
	if (NextIndex == Actions.Num())
	{
		Finish();
		return;
	}
	USkillAction* Child = Actions[NextIndex++];
	if (!Child)
	{
		Finish(false);
		return;
	}
	Child->OnFinished.AddUObject(this, &ThisClass::ChildFinished);
	Child->Start(GetAbility(), GetContext());
}

void USkillSequenceAction::ChildFinished(USkillAction* Child, bool bSucceeded)
{
	if (!IsRunning()) return;
	if (bSucceeded)
	{
		SetContext(Child->GetResultContext());
		StartNext();
	}
	else Finish(false);
}

void USkillSequenceAction::OnStop()
{
	for (USkillAction* Child : Actions)
	{
		if (!Child) continue;
		Child->OnFinished.RemoveAll(this);
		Child->Cancel();
	}
}

void USkillParallelAction::OnStart()
{
	Remaining = Actions.Num();
	if (Remaining == 0)
	{
		Finish();
		return;
	}
	for (USkillAction* Child : Actions)
	{
		if (!IsRunning()) break;
		if (!Child)
		{
			Finish(false);
			break;
		}
		Child->OnFinished.AddUObject(this, &ThisClass::ChildFinished);
		Child->Start(GetAbility(), GetContext());
	}
}

void USkillParallelAction::ChildFinished(USkillAction* Child, bool bSucceeded)
{
	if (!IsRunning()) return;
	if (!bSucceeded) Finish(false);
	else if (--Remaining == 0) Finish();
}

void USkillParallelAction::OnStop()
{
	for (USkillAction* Child : Actions)
	{
		if (!Child) continue;
		Child->OnFinished.RemoveAll(this);
		Child->Cancel();
	}
}
