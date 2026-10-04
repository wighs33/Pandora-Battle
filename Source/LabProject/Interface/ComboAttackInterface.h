#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ComboAttackInterface.generated.h"

UINTERFACE(meta = (CannotImplementInterfaceInBlueprint))
class LABPROJECT_API UComboAttackInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 콤보 입력을 받는 공격 능력의 계약.
 * 입력을 전달하고 서버에 맞춰 보내는 쪽(전투 컴포넌트)은 구체 능력 클래스 대신 이 계약만 안다.
 */
class LABPROJECT_API IComboAttackInterface
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	/** 지금 콤보 입력이 받아들여지면 넘어갈 몽타주 섹션. 클라이언트와 서버가 같은 섹션을 기대하는지 맞춰 보는 데 쓴다. */
	virtual FName GetNextAttackSectionName() const = 0;

	/** 콤보 입력을 받는다. 받아들였으면 true. */
	virtual bool RequestNextComboInput() = 0;

	/** 같은 실행이 자연 종료된 직후 들어온 입력을 한 번만 소비한다. 소비했으면 true. */
	virtual bool TryConsumeLateComboInput() = 0;
};
