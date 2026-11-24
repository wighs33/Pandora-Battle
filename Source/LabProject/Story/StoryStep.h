#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "StoryStep.generated.h"

class AStoryDirector;

/** 재생 중인 이야기 단계 하나. 단계(UStoryStep)는 데이터만 담고, 진행 상태는 여기에 둔다. */
struct FStoryTask
{
	virtual ~FStoryTask() = default;

	/** 할 일이 끝났으면 true. */
	virtual bool Tick(AStoryDirector& Director, float DeltaSeconds) = 0;
};

/** 정해 둔 시간만큼 기다린다. */
struct FStoryWaitTask : FStoryTask
{
	explicit FStoryWaitTask(const float InSeconds) : Remaining(InSeconds) {}

	virtual bool Tick(AStoryDirector& Director, const float DeltaSeconds) override
	{
		Remaining -= DeltaSeconds;
		return Remaining <= 0.f;
	}

private:
	float Remaining = 0.f;
};

/** 이야기의 한 단계(대사, 카메라, 동작, 이동 등). 장 정의(UStoryChapterDefinition)가 차례대로 담는다. */
UCLASS(Abstract, EditInlineNew, DefaultToInstanced, CollapseCategories)
class LABPROJECT_API UStoryStep : public UObject
{
	GENERATED_BODY()

public:
	/** 끝날 때까지 다음 단계로 넘어가지 않는다. 끄면 다음 단계와 함께 진행한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Step")
	bool bWait = true;

	/** 단계를 시작한다. 바로 끝나는 단계는 비어 있는 포인터를 돌려준다. */
	virtual TUniquePtr<FStoryTask> Start(AStoryDirector& Director) const PURE_VIRTUAL(UStoryStep::Start, return nullptr;);
};
