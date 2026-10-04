#include "AI/StateTree/StateTree_PdUtilityTasks.h"

#include "AI/Monster/MonsterAIController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "StateTreeExecutionContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StateTree_PdUtilityTasks)

#define LOCTEXT_NAMESPACE "LabProjectStateTree"

namespace
{
UCharacterMovementComponent* ResolveMovementComponent(
	const FStateTreePdMovementParametersTaskInstanceData& InstanceData)
{
	return
		IsValid(InstanceData.Pawn)
			? InstanceData.Pawn->GetCharacterMovement()
			: nullptr;
}

void CaptureAndApplyMovementParameters(
	FStateTreePdMovementParametersTaskInstanceData& InstanceData,
	UCharacterMovementComponent* Movement)
{
	if (InstanceData.bHasSavedMovementParameters)
	{
		return;
	}

	if (!IsValid(Movement))
	{
		InstanceData.SavedMovementComponent.Reset();
		InstanceData.bHasSavedMovementParameters = false;
		return;
	}

	InstanceData.SavedMovementComponent = Movement;
	InstanceData.SavedMaxWalkSpeed = Movement->MaxWalkSpeed;
	InstanceData.SavedRotationRate = Movement->RotationRate;
	InstanceData.SavedGroundFriction = Movement->GroundFriction;
	InstanceData.SavedMaxAcceleration = Movement->MaxAcceleration;
	InstanceData.bHasSavedMovementParameters = true;

	Movement->MaxWalkSpeed = InstanceData.WalkSpeedWhileActive;
	Movement->RotationRate = FRotator(0.0f, InstanceData.RotationRateWhileActive, 0.0f);
	Movement->GroundFriction = InstanceData.GroundFrictionWhileActive;
	Movement->MaxAcceleration = InstanceData.AccelerationWhileActive;
}

void RestoreMovementParameters(
	FStateTreePdMovementParametersTaskInstanceData& InstanceData)
{
	if (!InstanceData.bHasSavedMovementParameters)
	{
		return;
	}

	UCharacterMovementComponent* Movement = InstanceData.SavedMovementComponent.Get();
	if (IsValid(Movement))
	{
		Movement->MaxWalkSpeed = InstanceData.SavedMaxWalkSpeed;
		Movement->RotationRate = InstanceData.SavedRotationRate;
		Movement->GroundFriction = InstanceData.SavedGroundFriction;
		Movement->MaxAcceleration = InstanceData.SavedMaxAcceleration;
	}

	InstanceData.SavedMovementComponent.Reset();
	InstanceData.bHasSavedMovementParameters = false;
}

#if WITH_EDITOR
// 입력 하나에 연결된 이름이 있으면 그 이름을 넣은 문구를, 없으면 기본 문구를 에디터 설명으로 쓴다.
FText DescribeWithBoundInput(const FGuid& ID, const FName InputName, const IStateTreeBindingLookup& BindingLookup,
	const EStateTreeNodeFormatting Formatting, const FText& UnboundText, const FTextFormat& BoundFormat)
{
	const FText BoundName = BindingLookup.GetBindingSourceDisplayName(FPropertyBindingPath(ID, InputName), Formatting);
	return BoundName.IsEmpty() ? UnboundText : FText::Format(BoundFormat, BoundName);
}
#endif
}

FStateTreePdSaveLocationTask::FStateTreePdSaveLocationTask()
{
	bShouldCallTick = false;
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

EStateTreeRunStatus FStateTreePdSaveLocationTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	if (Transition.ChangeType == EStateTreeStateChangeType::Changed)
	{
		FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
		if (IsValid(InstanceData.Pawn))
		{
			InstanceData.CachedLocation = InstanceData.Pawn->GetActorLocation();
		}
	}

	return EStateTreeRunStatus::Running;
}

#if WITH_EDITOR
FText FStateTreePdSaveLocationTask::GetDescription(
	const FGuid& ID,
	FStateTreeDataView InstanceDataView,
	const IStateTreeBindingLookup& BindingLookup,
	EStateTreeNodeFormatting Formatting) const
{
	static_cast<void>(InstanceDataView);
	return DescribeWithBoundInput(ID, GET_MEMBER_NAME_CHECKED(FInstanceDataType, Pawn), BindingLookup, Formatting,
		LOCTEXT("SaveLocation", "Save the Actor's location on Enter State"),
		LOCTEXT("SaveLocationPawn", "Save {0}'s location on Enter State"));
}
#endif

FStateTreePdTrackPlayerTask::FStateTreePdTrackPlayerTask()
{
	bShouldCallTick = true;
	bShouldCopyBoundPropertiesOnTick = true;
	bShouldCopyBoundPropertiesOnExitState = false;
}

EStateTreeRunStatus FStateTreePdTrackPlayerTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	static_cast<void>(Transition);

	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (InstanceData.Controller)
	{
		InstanceData.Controller->RefreshPerceivedPlayerPawn();
	}
	InstanceData.TargetPlayerPawn = InstanceData.Controller
		? InstanceData.Controller->GetPerceivedPlayerPawn()
		: nullptr;
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreePdTrackPlayerTask::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	static_cast<void>(DeltaTime);

	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (InstanceData.Controller)
	{
		InstanceData.Controller->RefreshPerceivedPlayerPawn();
	}
	InstanceData.TargetPlayerPawn = InstanceData.Controller
		? InstanceData.Controller->GetPerceivedPlayerPawn()
		: nullptr;
	return EStateTreeRunStatus::Running;
}

#if WITH_EDITOR
FText FStateTreePdTrackPlayerTask::GetDescription(
	const FGuid& ID,
	FStateTreeDataView InstanceDataView,
	const IStateTreeBindingLookup& BindingLookup,
	EStateTreeNodeFormatting Formatting) const
{
	static_cast<void>(InstanceDataView);
	return DescribeWithBoundInput(ID, GET_MEMBER_NAME_CHECKED(FInstanceDataType, Controller), BindingLookup, Formatting,
		LOCTEXT("TrackPlayer", "Track perceived player"),
		LOCTEXT("TrackPlayerController", "Track perceived player from {0}"));
}
#endif

FStateTreePdMovementParametersTask::FStateTreePdMovementParametersTask()
{
	bShouldCallTick = false;
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

EStateTreeRunStatus FStateTreePdMovementParametersTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	if (Transition.ChangeType == EStateTreeStateChangeType::Changed)
	{
		FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
		CaptureAndApplyMovementParameters(InstanceData, ResolveMovementComponent(InstanceData));
	}

	return EStateTreeRunStatus::Running;
}

void FStateTreePdMovementParametersTask::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	if (Transition.ChangeType == EStateTreeStateChangeType::Changed)
	{
		RestoreMovementParameters(Context.GetInstanceData(*this));
	}
}

#if WITH_EDITOR
FText FStateTreePdMovementParametersTask::GetDescription(
	const FGuid& ID,
	FStateTreeDataView InstanceDataView,
	const IStateTreeBindingLookup& BindingLookup,
	EStateTreeNodeFormatting Formatting) const
{
	static_cast<void>(InstanceDataView);
	return DescribeWithBoundInput(ID, GET_MEMBER_NAME_CHECKED(FInstanceDataType, Pawn), BindingLookup, Formatting,
		LOCTEXT("MovementParameters", "Set movement parameters while state is active"),
		LOCTEXT("MovementParametersPawn", "Set movement parameters on {0}"));
}
#endif

#undef LOCTEXT_NAMESPACE
