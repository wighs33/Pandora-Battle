#include "AI/StateTree_PdUtilityTasks.h"

#include "AI/MonsterAIController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "StateTreeExecutionContext.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

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

	const FText PawnValue = BindingLookup.GetBindingSourceDisplayName(
		FPropertyBindingPath(ID, GET_MEMBER_NAME_CHECKED(FInstanceDataType, Pawn)),
		Formatting);
	return PawnValue.IsEmpty()
		? LOCTEXT("SaveLocation", "Save the Actor's location on Enter State")
		: FText::Format(LOCTEXT("SaveLocationPawn", "Save {0}'s location on Enter State"), PawnValue);
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

	const FText ControllerValue = BindingLookup.GetBindingSourceDisplayName(
		FPropertyBindingPath(ID, GET_MEMBER_NAME_CHECKED(FInstanceDataType, Controller)),
		Formatting);
	return ControllerValue.IsEmpty()
		? LOCTEXT("TrackPlayer", "Track perceived player")
		: FText::Format(LOCTEXT("TrackPlayerController", "Track perceived player from {0}"), ControllerValue);
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

	const FText PawnValue = BindingLookup.GetBindingSourceDisplayName(
		FPropertyBindingPath(ID, GET_MEMBER_NAME_CHECKED(FInstanceDataType, Pawn)),
		Formatting);
	return PawnValue.IsEmpty()
		? LOCTEXT("MovementParameters", "Set movement parameters while state is active")
		: FText::Format(LOCTEXT("MovementParametersPawn", "Set movement parameters on {0}"), PawnValue);
}
#endif

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FStateTreePdMovementParametersRestoreTest,
	"LabProject.AI.StateTree.MovementParametersRestore",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ClientContext
		| EAutomationTestFlags::CommandletContext
		| EAutomationTestFlags::EngineFilter)

bool FStateTreePdMovementParametersRestoreTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);

	UCharacterMovementComponent* Movement = NewObject<UCharacterMovementComponent>();
	if (!TestNotNull(TEXT("Movement component should be created"), Movement))
	{
		return false;
	}

	constexpr float OriginalWalkSpeed = 321.0f;
	const FRotator OriginalRotationRate(11.0f, 22.0f, 33.0f);
	constexpr float OriginalGroundFriction = 4.5f;
	constexpr float OriginalMaxAcceleration = 678.0f;

	Movement->MaxWalkSpeed = OriginalWalkSpeed;
	Movement->RotationRate = OriginalRotationRate;
	Movement->GroundFriction = OriginalGroundFriction;
	Movement->MaxAcceleration = OriginalMaxAcceleration;

	FStateTreePdMovementParametersTaskInstanceData OuterState;
	OuterState.WalkSpeedWhileActive = 500.0f;
	OuterState.RotationRateWhileActive = 90.0f;
	OuterState.GroundFrictionWhileActive = 0.5f;
	OuterState.AccelerationWhileActive = 500.0f;
	CaptureAndApplyMovementParameters(OuterState, Movement);

	FStateTreePdMovementParametersTaskInstanceData InnerState;
	InnerState.WalkSpeedWhileActive = 700.0f;
	InnerState.RotationRateWhileActive = 180.0f;
	InnerState.GroundFrictionWhileActive = 1.5f;
	InnerState.AccelerationWhileActive = 1200.0f;
	CaptureAndApplyMovementParameters(InnerState, Movement);

	RestoreMovementParameters(InnerState);
	TestEqual(TEXT("Inner exit should restore the outer walk speed"), Movement->MaxWalkSpeed, OuterState.WalkSpeedWhileActive);
	TestTrue(
		TEXT("Inner exit should restore the outer rotation rate"),
		Movement->RotationRate.Equals(FRotator(0.0f, OuterState.RotationRateWhileActive, 0.0f)));
	TestEqual(TEXT("Inner exit should restore the outer ground friction"), Movement->GroundFriction, OuterState.GroundFrictionWhileActive);
	TestEqual(TEXT("Inner exit should restore the outer acceleration"), Movement->MaxAcceleration, OuterState.AccelerationWhileActive);

	RestoreMovementParameters(OuterState);
	TestEqual(TEXT("Outer exit should restore the original walk speed"), Movement->MaxWalkSpeed, OriginalWalkSpeed);
	TestTrue(TEXT("Outer exit should restore the complete original rotation rate"), Movement->RotationRate.Equals(OriginalRotationRate));
	TestEqual(TEXT("Outer exit should restore the original ground friction"), Movement->GroundFriction, OriginalGroundFriction);
	TestEqual(TEXT("Outer exit should restore the original acceleration"), Movement->MaxAcceleration, OriginalMaxAcceleration);
	TestFalse(TEXT("Outer snapshot should be consumed after restoration"), OuterState.bHasSavedMovementParameters);
	TestFalse(TEXT("Inner snapshot should be consumed after restoration"), InnerState.bHasSavedMovementParameters);

	return true;
}
#endif

#undef LOCTEXT_NAMESPACE
