#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "UObject/PrimaryAssetId.h"
#include "ExperienceHostGameMode.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UExperienceHostGameMode : public UInterface
{
	GENERATED_BODY()
};

/**
 * 맵 월드 설정의 기본 Experience를 불러 플레이를 준비하는 게임 모드.
 * 프로젝트 맵이 이런 게임 모드를 쓰면 월드 설정에 DefaultExperienceId가 있어야 한다.
 */
class LABPROJECT_API IExperienceHostGameMode
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetConfiguredExperienceId() const = 0;
};
