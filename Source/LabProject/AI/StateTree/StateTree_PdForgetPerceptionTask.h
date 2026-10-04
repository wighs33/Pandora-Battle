#pragma once

#include "CoreMinimal.h"
#include "Tasks/StateTreeAITask.h"
#include "StateTree_PdForgetPerceptionTask.generated.h"

class AAIController;

USTRUCT()
struct FStateTreePdForgetPerceptionTaskInstanceData
{
	GENERATED_BODY()

	// 노드를 이 네이티브 태스크로 옮겨도 기존 StateTree 바인딩이 그대로 유효하도록
	// 원래 블루프린트 속성 이름을 유지한다.
	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<AAIController> Controller = nullptr;
};

/**
 * 대상이 설정한 시야 상실 반경 밖에 있을 때만 몬스터의 현재 대상을 지운다.
 * 대상이 가까이 있는 동안에는 공격 루프가 흔들리지 않고, 멀리 간 대상을 잊으면
 * StateTree가 배회로 돌아간다.
 *
 * 상태가 유지된 채 다시 선택될 때는 일부러 아무것도 하지 않는다.
 * 태스크는 계속 실행 중으로 남아, 감싸는 상태가 수명을 정한다.
 */
USTRUCT(meta = (DisplayName = "Pd Forget Distant Perception", Category = "AI"))
struct LABPROJECT_API FStateTreePdForgetPerceptionTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()

public:
	using FInstanceDataType = FStateTreePdForgetPerceptionTaskInstanceData;

	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup,
		EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;

	virtual FColor GetIconColor() const override
	{
		return FColor(90, 170, 255);
	}
#endif

	// Public API ------------------------------------------------------------------------------------------------------
	FStateTreePdForgetPerceptionTask();
};
