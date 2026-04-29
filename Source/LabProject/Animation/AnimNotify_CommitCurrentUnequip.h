#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"
#include "AnimNotify_CommitCurrentUnequip.generated.h"

/**
 * <장착 해제 커밋 애님 노티파이>
 * - 장착 해제 시점에 실제 장비 제거를 실행합니다.
 * - 장비 제거 성공 시 지정한 게임플레이 이벤트를 전송합니다.
 * - 서버 권한에서만 실제 장착 해제 처리를 수행합니다.
 */
UCLASS()
class LABPROJECT_API UAnimNotify_CommitCurrentUnequip : public UAnimNotify
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