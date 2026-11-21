#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GameplayTagContainer.h"
#include "AnimNotifyState_LooseGameplayTag.generated.h"

/**
 * 구간 동안 캐릭터 ASC에 태그를 붙인다. 몽타주가 끊겨도 구간이 끝나면 떼어 낸다.
 * 태그는 복제하지 않고, 몽타주가 재생되는 각 인스턴스가 스스로 붙인다(예: 날아오른 몬스터의 State.Monster.Flying).
 */
UCLASS(meta = (DisplayName = "Gameplay Tag While Active"))
class LABPROJECT_API UAnimNotifyState_LooseGameplayTag : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration,
		const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

protected:
	UPROPERTY(EditAnywhere, Category = "!Tag")
	FGameplayTag Tag;
};
