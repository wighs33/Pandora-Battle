#pragma once

#include "CoreMinimal.h"
#include "Tasks/StateTreeAITask.h"
#include "StateTree_PdUtilityTasks.generated.h"

class AActor;
class ACharacter;
class AMonsterAIController;
class APawn;
class UCharacterMovementComponent;

USTRUCT()
struct FStateTreePdSaveLocationTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<AActor> Pawn = nullptr;

	UPROPERTY(EditAnywhere, Category = Output)
	FVector CachedLocation = FVector::ZeroVector;
};

USTRUCT(meta = (DisplayName = "Pd Save Location", Category = "AI|Data"))
struct LABPROJECT_API FStateTreePdSaveLocationTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreePdSaveLocationTaskInstanceData;

	FStateTreePdSaveLocationTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(
		const FGuid& ID,
		FStateTreeDataView InstanceDataView,
		const IStateTreeBindingLookup& BindingLookup,
		EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};

USTRUCT()
struct FStateTreePdTrackPlayerTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<AMonsterAIController> Controller = nullptr;

	UPROPERTY(EditAnywhere, Category = Output)
	TObjectPtr<APawn> TargetPlayerPawn = nullptr;
};

/**
 * Publishes the player remembered by AMonsterAIController's native perception
 * route. Perception delegates are owned by the controller, so entering and
 * leaving this task cannot accumulate dynamic delegate bindings.
 */
USTRUCT(meta = (DisplayName = "Pd Track Perceived Player", Category = "AI|Perception"))
struct LABPROJECT_API FStateTreePdTrackPlayerTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreePdTrackPlayerTaskInstanceData;

	FStateTreePdTrackPlayerTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;

#if WITH_EDITOR
	virtual FText GetDescription(
		const FGuid& ID,
		FStateTreeDataView InstanceDataView,
		const IStateTreeBindingLookup& BindingLookup,
		EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};

USTRUCT()
struct FStateTreePdMovementParametersTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<ACharacter> Pawn = nullptr;

	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float WalkSpeedWhileActive = 500.0f;

	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0", ForceUnits = "deg/s"))
	float RotationRateWhileActive = 90.0f;

	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0"))
	float GroundFrictionWhileActive = 0.5f;

	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0", ForceUnits = "cm/s^2"))
	float AccelerationWhileActive = 500.0f;

	// Kept only so existing StateTree assets can deserialize their old values.
	// Runtime restoration uses the values captured when the task enters.
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "The movement values present on task entry are restored automatically."))
	float WalkSpeedOnExit = 200.0f;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "The movement values present on task entry are restored automatically."))
	float RotationRateOnExit = 360.0f;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "The movement values present on task entry are restored automatically."))
	float GroundFrictionOnExit = 8.0f;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "The movement values present on task entry are restored automatically."))
	float AccelerationOnExit = 5000.0f;

	UPROPERTY(Transient)
	TWeakObjectPtr<UCharacterMovementComponent> SavedMovementComponent;

	UPROPERTY(Transient)
	float SavedMaxWalkSpeed = 0.0f;

	UPROPERTY(Transient)
	FRotator SavedRotationRate = FRotator::ZeroRotator;

	UPROPERTY(Transient)
	float SavedGroundFriction = 0.0f;

	UPROPERTY(Transient)
	float SavedMaxAcceleration = 0.0f;

	UPROPERTY(Transient)
	bool bHasSavedMovementParameters = false;
};

/**
 * Applies configured movement values while its state is active, then restores
 * the values captured from the same movement component on state entry.
 */
USTRUCT(meta = (DisplayName = "Pd Movement Parameters", Category = "AI|Movement"))
struct LABPROJECT_API FStateTreePdMovementParametersTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreePdMovementParametersTaskInstanceData;

	FStateTreePdMovementParametersTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(
		const FGuid& ID,
		FStateTreeDataView InstanceDataView,
		const IStateTreeBindingLookup& BindingLookup,
		EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};
