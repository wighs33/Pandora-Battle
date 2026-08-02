#include "AI/StateTree_PdMonsterAttackTask.h"

#include "AIController.h"
#include "Character/EnemyBase.h"
#include "Navigation/PathFollowingComponent.h"
#include "StateTreeExecutionContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StateTree_PdMonsterAttackTask)

#define LOCTEXT_NAMESPACE "LabProjectStateTree"

DEFINE_LOG_CATEGORY_STATIC(LogStateTreeMonsterAttack, Log, All);

namespace
{
constexpr float MoveRequestRetryInterval = 0.25f;

bool HasReachedTimeout(const float ElapsedTime, const float Timeout)
{
	return Timeout > 0.0f && ElapsedTime >= Timeout;
}
}

FStateTreePdMonsterAttackTask::FStateTreePdMonsterAttackTask()
{
	bShouldCallTick = true;
	bShouldCopyBoundPropertiesOnTick = true;
	bShouldCopyBoundPropertiesOnExitState = false;
}

EStateTreeRunStatus FStateTreePdMonsterAttackTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	static_cast<void>(Transition);

	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.bMoveRequested = false;
	InstanceData.bWaitingForAttackStart = false;
	InstanceData.bObservedAttackInProgress = false;
	InstanceData.AttackRetryTimeRemaining = 0.0f;
	InstanceData.MoveRetryTimeRemaining = 0.0f;
	InstanceData.AttackStartElapsedTime = 0.0f;
	InstanceData.AttackCompletionElapsedTime = 0.0f;

	return Tick(Context, 0.0f);
}

EStateTreeRunStatus FStateTreePdMonsterAttackTask::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIController* AIController = InstanceData.AIController;
	AEnemyBase* Enemy = AIController ? Cast<AEnemyBase>(AIController->GetPawn()) : nullptr;
	AActor* TargetActor = InstanceData.TargetActor;
	if (!IsValid(AIController)
		|| !IsValid(Enemy)
		|| !Enemy->IsActorValidAttackTarget(TargetActor))
	{
		return EStateTreeRunStatus::Failed;
	}

	const float SafeDeltaTime = FMath::Max(DeltaTime, 0.0f);
	const bool bAttackInProgress = Enemy->IsAttackInProgress();
	if (InstanceData.bObservedAttackInProgress)
	{
		if (!bAttackInProgress)
		{
			return EStateTreeRunStatus::Succeeded;
		}

		InstanceData.AttackCompletionElapsedTime += SafeDeltaTime;
		if (HasReachedTimeout(
			InstanceData.AttackCompletionElapsedTime,
			InstanceData.AttackCompletionTimeout))
		{
			UE_LOG(
				LogStateTreeMonsterAttack,
				Warning,
				TEXT("%s's attack against %s did not finish within %.2f seconds."),
				*GetNameSafe(Enemy),
				*GetNameSafe(TargetActor),
				InstanceData.AttackCompletionTimeout);
			return EStateTreeRunStatus::Failed;
		}

		return EStateTreeRunStatus::Running;
	}

	if (bAttackInProgress)
	{
		if (InstanceData.bStopMovementBeforeAttack)
		{
			AIController->StopMovement();
		}

		InstanceData.bMoveRequested = false;
		InstanceData.bObservedAttackInProgress = true;
		InstanceData.AttackCompletionElapsedTime = 0.0f;
		return EStateTreeRunStatus::Running;
	}

	const float DistanceToTarget = Enemy->GetAttackDistanceToActor(TargetActor);
	const float AttackStartDistance = Enemy->GetAttackStartDistance();
	if (InstanceData.bRequireTargetInAttackRange && DistanceToTarget > AttackStartDistance)
	{
		InstanceData.bWaitingForAttackStart = false;
		InstanceData.AttackRetryTimeRemaining = 0.0f;
		InstanceData.AttackStartElapsedTime = 0.0f;
		InstanceData.MoveRetryTimeRemaining =
			FMath::Max(InstanceData.MoveRetryTimeRemaining - SafeDeltaTime, 0.0f);

		if (!InstanceData.bMoveToTargetWhenOutOfRange)
		{
			return EStateTreeRunStatus::Failed;
		}

		const bool bNeedsMoveRequest = !InstanceData.bMoveRequested
			|| (AIController->GetMoveStatus() != EPathFollowingStatus::Moving
				&& InstanceData.MoveRetryTimeRemaining <= 0.0f);
		if (bNeedsMoveRequest)
		{
			if (!Enemy->RequestMoveToAttackTarget(TargetActor))
			{
				return EStateTreeRunStatus::Failed;
			}

			InstanceData.bMoveRequested = true;
			InstanceData.MoveRetryTimeRemaining = MoveRequestRetryInterval;
		}

		return EStateTreeRunStatus::Running;
	}

	if (InstanceData.bMoveRequested || InstanceData.bStopMovementBeforeAttack)
	{
		AIController->StopMovement();
	}
	InstanceData.bMoveRequested = false;
	InstanceData.MoveRetryTimeRemaining = 0.0f;

	if (!InstanceData.bWaitingForAttackStart)
	{
		InstanceData.bWaitingForAttackStart = true;
		InstanceData.AttackStartElapsedTime = 0.0f;
		InstanceData.AttackRetryTimeRemaining = 0.0f;
	}
	else
	{
		InstanceData.AttackStartElapsedTime += SafeDeltaTime;
	}

	InstanceData.AttackRetryTimeRemaining =
		FMath::Max(InstanceData.AttackRetryTimeRemaining - SafeDeltaTime, 0.0f);
	if (Enemy->IsAttackEnabled() && InstanceData.AttackRetryTimeRemaining <= 0.0f)
	{
		Enemy->SetAttackTarget(TargetActor);
		Enemy->Attack();
		InstanceData.AttackRetryTimeRemaining =
			FMath::Max(InstanceData.AttackRetryInterval, 0.0f);

		if (Enemy->IsAttackInProgress())
		{
			InstanceData.bObservedAttackInProgress = true;
			InstanceData.AttackCompletionElapsedTime = 0.0f;
			return EStateTreeRunStatus::Running;
		}
	}

	if (HasReachedTimeout(InstanceData.AttackStartElapsedTime, InstanceData.AttackStartTimeout))
	{
		UE_LOG(
			LogStateTreeMonsterAttack,
			Warning,
			TEXT("%s could not start an attack against %s within %.2f seconds."),
			*GetNameSafe(Enemy),
			*GetNameSafe(TargetActor),
			InstanceData.AttackStartTimeout);
		return EStateTreeRunStatus::Failed;
	}

	return EStateTreeRunStatus::Running;
}

void FStateTreePdMonsterAttackTask::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	static_cast<void>(Transition);

	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (InstanceData.bMoveRequested && IsValid(InstanceData.AIController))
	{
		InstanceData.AIController->StopMovement();
	}

	InstanceData.bMoveRequested = false;
}

#if WITH_EDITOR
FText FStateTreePdMonsterAttackTask::GetDescription(
	const FGuid& ID,
	FStateTreeDataView InstanceDataView,
	const IStateTreeBindingLookup& BindingLookup,
	EStateTreeNodeFormatting Formatting) const
{
	static_cast<void>(InstanceDataView);

	const FText TargetValue = BindingLookup.GetBindingSourceDisplayName(
		FPropertyBindingPath(ID, GET_MEMBER_NAME_CHECKED(FInstanceDataType, TargetActor)),
		Formatting);

	if (Formatting == EStateTreeNodeFormatting::RichText)
	{
		return TargetValue.IsEmpty()
			? LOCTEXT("MonsterAttackRich", "<b>Pd Monster Attack</>")
			: FText::Format(LOCTEXT("MonsterAttackTargetRich", "<b>Pd Monster Attack</> {0}"), TargetValue);
	}

	return TargetValue.IsEmpty()
		? LOCTEXT("MonsterAttack", "Pd Monster Attack")
		: FText::Format(LOCTEXT("MonsterAttackTarget", "Pd Monster Attack {0}"), TargetValue);
}
#endif

#undef LOCTEXT_NAMESPACE
