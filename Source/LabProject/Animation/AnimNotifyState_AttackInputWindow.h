#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GameplayTagContainer.h"
#include "AnimNotifyState_AttackInputWindow.generated.h"

/**
 * <공격 입력 가능 구간 노티파이 스테이트>
 * - 공격 입력 가능 시작/종료 시점에 게임플레이 이벤트를 전송합니다.
 * - 시작 태그와 종료 태그를 각각 분리해서 설정할 수 있습니다.
 * - 공격 연계 입력 가능 구간을 애니메이션 구간과 연결할 때 사용합니다.
 */
UCLASS()
class LABPROJECT_API UAnimNotifyState_AttackInputWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	/** 노티파이 시작 시점 */
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration,
		const FAnimNotifyEventReference& EventReference) override;

	/** 노티파이 종료 시점 */
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	/** 에디터와 디버그에서 표시할 노티파이 이름 */
	virtual FString GetNotifyName_Implementation() const override;

protected:
	// 시작
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Tag")
	FGameplayTag StartEventTag;

	// 종료
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Tag")
	FGameplayTag EndEventTag;
};