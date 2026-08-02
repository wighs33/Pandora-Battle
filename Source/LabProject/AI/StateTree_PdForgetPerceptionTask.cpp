#include "AI/StateTree_PdForgetPerceptionTask.h"

#include "AI/MonsterAIController.h"
#include "AIController.h"
#include "Perception/AIPerceptionComponent.h"
#include "StateTreeExecutionContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StateTree_PdForgetPerceptionTask)

#define LOCTEXT_NAMESPACE "LabProjectStateTree"

FStateTreePdForgetPerceptionTask::FStateTreePdForgetPerceptionTask()
{
	bShouldStateChangeOnReselect = true;
	bShouldCallTick = false;
	bShouldCallTickOnlyOnEvents = false;
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

EStateTreeRunStatus FStateTreePdForgetPerceptionTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	if (Transition.ChangeType != EStateTreeStateChangeType::Changed)
	{
		return EStateTreeRunStatus::Running;
	}

	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (AAIController* Controller = InstanceData.Controller)
	{
		if (AMonsterAIController* MonsterController = Cast<AMonsterAIController>(Controller))
		{
			MonsterController->ForgetPerceivedPlayerIfOutOfRange();
		}
		else if (UAIPerceptionComponent* PerceptionComponent = Controller->GetAIPerceptionComponent())
		{
			PerceptionComponent->ForgetAll();
		}
	}

	return EStateTreeRunStatus::Running;
}

#if WITH_EDITOR
FText FStateTreePdForgetPerceptionTask::GetDescription(
	const FGuid& ID,
	FStateTreeDataView InstanceDataView,
	const IStateTreeBindingLookup& BindingLookup,
	const EStateTreeNodeFormatting Formatting) const
{
	static_cast<void>(InstanceDataView);

	const FText ControllerValue = BindingLookup.GetBindingSourceDisplayName(
		FPropertyBindingPath(ID, GET_MEMBER_NAME_CHECKED(FInstanceDataType, Controller)),
		Formatting);

	if (Formatting == EStateTreeNodeFormatting::RichText)
	{
		return ControllerValue.IsEmpty()
			? LOCTEXT("ForgetPerceptionRich", "<b>Forget Distant Perception</>")
			: FText::Format(
				LOCTEXT("ForgetPerceptionControllerRich", "<b>Forget Distant Perception</> on {0}"),
				ControllerValue);
	}

	return ControllerValue.IsEmpty()
		? LOCTEXT("ForgetPerception", "Forget Distant Perception")
		: FText::Format(
			LOCTEXT("ForgetPerceptionController", "Forget Distant Perception on {0}"),
			ControllerValue);
}
#endif

#undef LOCTEXT_NAMESPACE
