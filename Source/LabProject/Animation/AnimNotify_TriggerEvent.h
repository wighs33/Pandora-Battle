// 트리거 이벤트 노티파이

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"
#include "AnimNotify_TriggerEvent.generated.h"

/**
 * <이벤트 트리거 애님 노티파이>
 * - 지정한 게임플레이 이벤트 태그를 액터에 전송합니다.
 * - AbilitySystemComponent가 있는 액터만 대상으로 처리합니다.
 * - 애니메이션 시점과 GAS 이벤트를 연결할 때 사용합니다.
 */
UCLASS()
class LABPROJECT_API UAnimNotify_TriggerEvent : public UAnimNotify
{
	GENERATED_BODY()

public:
	/** 애님 노티파이 시점 */
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	/** 에디터와 디버그에서 표시할 노티파이 이름 */
	virtual FString GetNotifyName_Implementation() const override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Tag")
	FGameplayTag EventTag;
};