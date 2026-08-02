#pragma once

#include "CoreMinimal.h"
#include "Tasks/StateTreeAITask.h"
#include "StateTree_PdForgetPerceptionTask.generated.h"

class AAIController;

USTRUCT()
struct FStateTreePdForgetPerceptionTaskInstanceData
{
	GENERATED_BODY()

	// Keep the original Blueprint property name so existing StateTree bindings
	// remain valid when the node is migrated to this native task.
	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<AAIController> Controller = nullptr;
};

/**
 * Clears the monster's current target only when it is beyond the configured
 * lose-sight radius. This keeps the attack loop stable while the target is
 * nearby, and lets the StateTree return to roaming after a distant target is
 * forgotten.
 *
 * A sustained reselect intentionally does nothing.
 * The task remains running so the containing state controls its lifetime.
 */
USTRUCT(meta = (DisplayName = "Pd Forget Distant Perception", Category = "AI"))
struct LABPROJECT_API FStateTreePdForgetPerceptionTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreePdForgetPerceptionTaskInstanceData;

	FStateTreePdForgetPerceptionTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(
		const FGuid& ID,
		FStateTreeDataView InstanceDataView,
		const IStateTreeBindingLookup& BindingLookup,
		EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;

	virtual FColor GetIconColor() const override
	{
		return FColor(90, 170, 255);
	}
#endif
};
