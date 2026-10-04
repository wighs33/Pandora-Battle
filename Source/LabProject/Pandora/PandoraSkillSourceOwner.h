#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PandoraSkillSourceOwner.generated.h"

class UPandoraSkillSource;

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UPandoraSkillSourceOwner : public UInterface
{
	GENERATED_BODY()
};

/**
 * 스킬 원본을 서브오브젝트로 만들어 복제하는 쪽. 원본은 바깥 객체 중 이 인터페이스를 찾아 알린다.
 * 원본이 소유자 클래스를 몰라도 되므로 Pandora 폴더가 컴포넌트 폴더를 참조하지 않는다.
 */
class LABPROJECT_API IPandoraSkillSourceOwner
{
	GENERATED_BODY()

public:
	// 클라이언트에서 원본 값이 도착했을 때.
	virtual void HandleSkillSourceReplicated(UPandoraSkillSource* Source) = 0;
	// 서버가 원본을 지워 클라이언트 사본이 사라지기 직전.
	virtual void HandleSkillSourceDestroyed(UPandoraSkillSource* Source) = 0;
};
