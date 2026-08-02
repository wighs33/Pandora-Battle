#pragma once

#include "CoreMinimal.h"
#include "Conditions/StateTreeAIConditionBase.h"
#include "StateTree_PdDistanceConditions.generated.h"

class AActor;

USTRUCT()
struct FStateTreePdPlayerDistanceConditionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<AActor> Actor = nullptr;

	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<AActor> TargetActor = nullptr;

	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0", ForceUnits = "cm"))
	double TriggerDistance = 0.0;
};

/**
 * Tests the flat distance between an actor and the StateTree's actual combat
 * target. A missing target does not pass the condition, allowing target loss
 * to select the roaming state.
 */
USTRUCT(meta = (DisplayName = "Pd Target Actor Distance", Category = "AI|Distance"))
struct LABPROJECT_API FStateTreePdPlayerDistanceCondition : public FStateTreeAIConditionBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreePdPlayerDistanceConditionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

#if WITH_EDITOR
	virtual FText GetDescription(
		const FGuid& ID,
		FStateTreeDataView InstanceDataView,
		const IStateTreeBindingLookup& BindingLookup,
		EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};

USTRUCT()
struct FStateTreePdTargetDistanceConditionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<AActor> Actor = nullptr;

	UPROPERTY(EditAnywhere, Category = Input)
	FVector TargetLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = Input, meta = (ClampMin = "0.0", ForceUnits = "cm"))
	double Distance = 0.0;

	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bFartherAway = true;
};

/**
 * Tests whether an actor is farther from, or closer to, a target location in
 * the XY plane. This is the native replacement for STC_TargetDistance.
 */
USTRUCT(meta = (DisplayName = "Pd Target Distance", Category = "AI|Distance"))
struct LABPROJECT_API FStateTreePdTargetDistanceCondition : public FStateTreeAIConditionBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreePdTargetDistanceConditionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

#if WITH_EDITOR
	virtual FText GetDescription(
		const FGuid& ID,
		FStateTreeDataView InstanceDataView,
		const IStateTreeBindingLookup& BindingLookup,
		EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};
