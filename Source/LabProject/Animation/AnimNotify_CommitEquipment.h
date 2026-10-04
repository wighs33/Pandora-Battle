#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"
#include "AnimNotify_CommitEquipment.generated.h"

class UEquipmentComponent;

/**
 * 장착·해제 몽타주의 확정 시점에 서버에서 장비 변경을 확정하고, 성공하면 EventTag 게임플레이 이벤트를 보낸다.
 * 무엇을 확정할지는 파생 클래스가 정한다.
 */
UCLASS(Abstract)
class LABPROJECT_API UAnimNotify_CommitEquipment : public UAnimNotify
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

protected:
	/** 장비 변경을 확정하고 성공 여부를 돌려준다. */
	virtual bool CommitEquipment(UEquipmentComponent& EquipmentComponent) const PURE_VIRTUAL(UAnimNotify_CommitEquipment::CommitEquipment, return false;);

	/** 몽타주 편집기에 보이는 노티파이 이름. */
	virtual FString GetCommitName() const PURE_VIRTUAL(UAnimNotify_CommitEquipment::GetCommitName, return FString(););

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Tag")
	FGameplayTag EventTag;
};
