#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "AttackTargetSourceInterface.generated.h"

class AActor;

UINTERFACE(meta = (CannotImplementInterfaceInBlueprint))
class LABPROJECT_API UAttackTargetSourceInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 적이 공격할 대상을 고르고 보관하는 쪽(AI 컨트롤러)의 계약.
 * 캐릭터와 공격 능력은 대상을 따로 들고 있지 않고 조종 중인 컨트롤러에게 묻는다.
 */
class LABPROJECT_API IAttackTargetSourceInterface
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	/** 지금 고른 공격 대상. 공격 가능 여부는 묻는 쪽이 다시 확인한다. */
	virtual AActor* GetSelectedAttackTarget() const = 0;
};
