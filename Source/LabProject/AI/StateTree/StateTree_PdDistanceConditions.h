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
 * 액터와 StateTree의 실제 전투 대상 사이의 평면 거리를 검사한다.
 * 대상이 없으면 조건을 통과하지 않으므로, 대상을 잃으면 배회 상태가 선택된다.
 */
USTRUCT(meta = (DisplayName = "Pd Target Actor Distance", Category = "AI|Distance"))
struct LABPROJECT_API FStateTreePdPlayerDistanceCondition : public FStateTreeAIConditionBase
{
	GENERATED_BODY()

public:
	using FInstanceDataType = FStateTreePdPlayerDistanceConditionInstanceData;

	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup,
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
 * 액터가 XY 평면에서 대상 위치보다 먼지 가까운지 검사한다.
 * STC_TargetDistance를 대신하는 네이티브 조건이다.
 */
USTRUCT(meta = (DisplayName = "Pd Target Distance", Category = "AI|Distance"))
struct LABPROJECT_API FStateTreePdTargetDistanceCondition : public FStateTreeAIConditionBase
{
	GENERATED_BODY()

public:
	using FInstanceDataType = FStateTreePdTargetDistanceConditionInstanceData;

	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup,
		EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};
