#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_SpawnAreaStrikes.generated.h"

class AMonsterAreaStrike;

/**
 * 서버: 몬스터 둘레에 범위 공격(메테오)을 떨어뜨린다.
 * PlayerSearchRadius 안의 공격 대상마다 발밑에 하나씩, 그리고 몬스터 둘레 ExtraStrikeRadius 안의 바닥에 ExtraStrikeCount개를 더 놓는다.
 * 대상 발밑에 놓인 것은 움직여야 피할 수 있고, 둘레에 흩어진 것은 피할 자리를 좁힌다.
 */
UCLASS(meta = (DisplayName = "Spawn Area Strikes"))
class LABPROJECT_API UAnimNotify_SpawnAreaStrikes : public UAnimNotify
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

protected:
	UPROPERTY(EditAnywhere, Category = "!Strike")
	TSubclassOf<AMonsterAreaStrike> StrikeClass;

	UPROPERTY(EditAnywhere, Category = "!Strike", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float PlayerSearchRadius = 3000.0f;

	UPROPERTY(EditAnywhere, Category = "!Strike", meta = (ClampMin = "0"))
	int32 ExtraStrikeCount = 4;

	/** 둘레에 흩어 놓는 거리의 범위. 몬스터 몸 아래는 비워 두도록 최소 거리를 둔다. */
	UPROPERTY(EditAnywhere, Category = "!Strike", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float ExtraStrikeMinDistance = 600.0f;

	UPROPERTY(EditAnywhere, Category = "!Strike", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float ExtraStrikeRadius = 1800.0f;
};
