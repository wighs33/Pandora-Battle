#pragma once

#include "CoreMinimal.h"
#include "Engine/TargetPoint.h"
#include "StoryMark.generated.h"

/** 무대의 자리 표시. 연출이 이름으로 찾아 인물을 세우거나 걷게 한다. */
UCLASS()
class LABPROJECT_API AStoryMark : public ATargetPoint
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Story")
	FName MarkId;
};
