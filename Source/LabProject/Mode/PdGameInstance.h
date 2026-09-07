#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "PdGameInstance.generated.h"

/**
 * 게임 시작 시 필요한 전역 서비스의 실행을 연결한다.
 *
 * 저장, 로비 전환 데이터, 콘텐츠 로딩과 BGM의 상태 및 정리는
 * 각 GameInstance 서브시스템이 담당한다.
 */
UCLASS(BlueprintType, Blueprintable, Config=Game)
class LABPROJECT_API UPdGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	// Engine Callbacks
	virtual void OnStart() override;
};
