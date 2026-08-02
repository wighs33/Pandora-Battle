#pragma once

#include "CoreMinimal.h"
#include "Tasks/StateTreeAITask.h"
#include "StateTree_PdMonsterAttackTask.generated.h"

class AActor;
class AAIController;

USTRUCT()
struct FStateTreePdMonsterAttackTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<AAIController> AIController = nullptr;

	UPROPERTY(EditAnywhere, Category = Parameter)
	TObjectPtr<AActor> TargetActor = nullptr;

	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bRequireTargetInAttackRange = true;

	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bMoveToTargetWhenOutOfRange = true;

	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bStopMovementBeforeAttack = true;

	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0", ForceUnits = "s"))
	float AttackRetryInterval = 0.1f;

	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0", ForceUnits = "s",
		ToolTip = "Maximum time to wait for an attack to start while in range. Zero waits indefinitely."))
	float AttackStartTimeout = 10.0f;

	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0", ForceUnits = "s",
		ToolTip = "Maximum time to wait for an active attack to finish. Zero waits indefinitely."))
	float AttackCompletionTimeout = 10.0f;

	UPROPERTY(Transient)
	bool bMoveRequested = false;

	UPROPERTY(Transient)
	bool bWaitingForAttackStart = false;

	UPROPERTY(Transient)
	bool bObservedAttackInProgress = false;

	UPROPERTY(Transient)
	float AttackRetryTimeRemaining = 0.0f;

	UPROPERTY(Transient)
	float MoveRetryTimeRemaining = 0.0f;

	UPROPERTY(Transient)
	float AttackStartElapsedTime = 0.0f;

	UPROPERTY(Transient)
	float AttackCompletionElapsedTime = 0.0f;
};

USTRUCT(meta = (DisplayName = "Pd Monster Attack", Category = "AI|Action"))
struct LABPROJECT_API FStateTreePdMonsterAttackTask : public FStateTreeAIActionTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreePdMonsterAttackTaskInstanceData;

	FStateTreePdMonsterAttackTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(
		const FGuid& ID,
		FStateTreeDataView InstanceDataView,
		const IStateTreeBindingLookup& BindingLookup,
		EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;

	virtual FName GetIconName() const override
	{
		return FName("StateTreeEditorStyle|Node.Task");
	}

	virtual FColor GetIconColor() const override
	{
		return FColor(220, 80, 80);
	}
#endif
};
