#include "AI/StateTree/StateTree_PdMonsterAttackTask.h"

#include "AIController.h"
#include "AI/Monster/MonsterCharacter.h"
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

using FAttackTaskData = FStateTreePdMonsterAttackTaskInstanceData;

// 이미 시작한 공격이 끝나기를 기다린다. 끝나면 성공, 완료 제한 시간을 넘기면 실패.
EStateTreeRunStatus WaitForAttackToFinish(FAttackTaskData& InstanceData, const AEnemyBase& Enemy, const AActor* TargetActor,
	const bool bAttackInProgress, const float DeltaTime)
{
	if (!bAttackInProgress)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	InstanceData.AttackCompletionElapsedTime += DeltaTime;
	if (HasReachedTimeout(InstanceData.AttackCompletionElapsedTime, InstanceData.AttackCompletionTimeout))
	{
		UE_LOG(LogStateTreeMonsterAttack, Warning, TEXT("%s's attack against %s did not finish within %.2f seconds."),
			*GetNameSafe(&Enemy), *GetNameSafe(TargetActor), InstanceData.AttackCompletionTimeout);
		return EStateTreeRunStatus::Failed;
	}
	return EStateTreeRunStatus::Running;
}

// 사거리 밖이면 대상 쪽으로 이동을 요청하고, 이동이 멈춘 채 재요청 간격이 지나면 다시 요청한다.
EStateTreeRunStatus ApproachTarget(FAttackTaskData& InstanceData, const AAIController& AIController, AEnemyBase& Enemy,
	AActor* TargetActor, const float DeltaTime)
{
	InstanceData.bWaitingForAttackStart = false;
	InstanceData.AttackRetryTimeRemaining = 0.0f;
	InstanceData.AttackStartElapsedTime = 0.0f;
	InstanceData.MoveRetryTimeRemaining = FMath::Max(InstanceData.MoveRetryTimeRemaining - DeltaTime, 0.0f);
	if (!InstanceData.bMoveToTargetWhenOutOfRange)
	{
		return EStateTreeRunStatus::Failed;
	}

	const bool bNeedsMoveRequest = !InstanceData.bMoveRequested
		|| (AIController.GetMoveStatus() != EPathFollowingStatus::Moving && InstanceData.MoveRetryTimeRemaining <= 0.0f);
	if (bNeedsMoveRequest)
	{
		if (!Enemy.RequestMoveToAttackTarget(TargetActor))
		{
			return EStateTreeRunStatus::Failed;
		}
		InstanceData.bMoveRequested = true;
		InstanceData.MoveRetryTimeRemaining = MoveRequestRetryInterval;
	}
	return EStateTreeRunStatus::Running;
}

// 사거리 안에서는 이동을 멈추고 공격 간격마다 공격을 시도한다. 시작 제한 시간 안에 공격이 시작되지 않으면 실패.
EStateTreeRunStatus TryStartAttack(FAttackTaskData& InstanceData, AAIController& AIController, AEnemyBase& Enemy,
	const AActor* TargetActor, const float DeltaTime)
{
	if (InstanceData.bMoveRequested || InstanceData.bStopMovementBeforeAttack)
	{
		AIController.StopMovement();
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
		InstanceData.AttackStartElapsedTime += DeltaTime;
	}

	InstanceData.AttackRetryTimeRemaining = FMath::Max(InstanceData.AttackRetryTimeRemaining - DeltaTime, 0.0f);
	if (Enemy.IsAttackEnabled() && InstanceData.AttackRetryTimeRemaining <= 0.0f)
	{
		Enemy.Attack();
		InstanceData.AttackRetryTimeRemaining = FMath::Max(InstanceData.AttackRetryInterval, 0.0f);
		if (Enemy.IsAttackInProgress())
		{
			InstanceData.bObservedAttackInProgress = true;
			InstanceData.AttackCompletionElapsedTime = 0.0f;
			return EStateTreeRunStatus::Running;
		}
	}

	if (HasReachedTimeout(InstanceData.AttackStartElapsedTime, InstanceData.AttackStartTimeout))
	{
		UE_LOG(LogStateTreeMonsterAttack, Warning, TEXT("%s could not start an attack against %s within %.2f seconds."),
			*GetNameSafe(&Enemy), *GetNameSafe(TargetActor), InstanceData.AttackStartTimeout);
		return EStateTreeRunStatus::Failed;
	}
	return EStateTreeRunStatus::Running;
}
}

FStateTreePdMonsterAttackTask::FStateTreePdMonsterAttackTask()
{
	bShouldCallTick = true;
	bShouldCopyBoundPropertiesOnTick = true;
	bShouldCopyBoundPropertiesOnExitState = false;
}

EStateTreeRunStatus FStateTreePdMonsterAttackTask::EnterState(FStateTreeExecutionContext& Context,
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

EStateTreeRunStatus FStateTreePdMonsterAttackTask::Tick(FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIController* AIController = InstanceData.AIController;
	AEnemyBase* Enemy = AIController ? Cast<AEnemyBase>(AIController->GetPawn()) : nullptr;
	AActor* TargetActor = InstanceData.TargetActor;
	if (!IsValid(AIController) || !IsValid(Enemy) || !Enemy->IsActorValidAttackTarget(TargetActor))
	{
		return EStateTreeRunStatus::Failed;
	}

	const float SafeDeltaTime = FMath::Max(DeltaTime, 0.0f);
	const bool bAttackInProgress = Enemy->IsAttackInProgress();
	if (InstanceData.bObservedAttackInProgress)
	{
		return WaitForAttackToFinish(InstanceData, *Enemy, TargetActor, bAttackInProgress, SafeDeltaTime);
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

	const bool bOutOfRange = InstanceData.bRequireTargetInAttackRange
		&& Enemy->GetAttackDistanceToActor(TargetActor) > Enemy->GetAttackStartDistance();
	return bOutOfRange
		? ApproachTarget(InstanceData, *AIController, *Enemy, TargetActor, SafeDeltaTime)
		: TryStartAttack(InstanceData, *AIController, *Enemy, TargetActor, SafeDeltaTime);
}

void FStateTreePdMonsterAttackTask::ExitState(FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	static_cast<void>(Transition);

	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	// 공격 상태를 벗어난 뒤 몬스터 전용 공격의 모션·타격 창이 뒤늦게 남지 않게 한다.
	if (AMonsterCharacter* Monster = InstanceData.AIController ? Cast<AMonsterCharacter>(InstanceData.AIController->GetPawn()) : nullptr)
	{
		Monster->StopMonsterAttack();
	}
	if (InstanceData.bMoveRequested && IsValid(InstanceData.AIController))
	{
		InstanceData.AIController->StopMovement();
	}

	InstanceData.bMoveRequested = false;
}

#if WITH_EDITOR
FText FStateTreePdMonsterAttackTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView,
	const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	static_cast<void>(InstanceDataView);

	const FText TargetValue = BindingLookup.GetBindingSourceDisplayName(
		FPropertyBindingPath(ID, GET_MEMBER_NAME_CHECKED(FInstanceDataType, TargetActor)), Formatting);

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
