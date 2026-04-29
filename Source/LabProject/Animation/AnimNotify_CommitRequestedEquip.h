#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"
#include "AnimNotify_CommitRequestedEquip.generated.h"

/**
 * <요청 장착 커밋 애님 노티파이>
 * - 요청된 장비를 실제로 장착하는 시점을 담당합니다.
 * - 장착 성공 시 지정한 게임플레이 이벤트를 전송합니다.
 * - 서버 권한에서만 실제 장착 처리를 수행합니다.
 */
UCLASS()
class LABPROJECT_API UAnimNotify_CommitRequestedEquip : public UAnimNotify
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