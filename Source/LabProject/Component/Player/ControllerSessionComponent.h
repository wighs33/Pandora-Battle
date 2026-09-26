#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "ControllerSessionComponent.generated.h"

class APdPlayerController;
struct FGameResultPresentationData;

/**
 * 클라이언트·서버의 경기 퇴장과 타이틀 이동을 조율한다.
 *
 * 서버 권한의 경기 판단은 AExperienceGameMode가 담당하며, 이 컴포넌트는
 * 컨트롤러 RPC와 로컬 온라인 세션 정리를 연결한다.
 */
UCLASS(ClassGroup = (PlayerController))
class LABPROJECT_API UControllerSessionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	UControllerSessionComponent();

	bool RequestExitMatchToTitle();
	void TravelToTitleWithGameResult(
		const FGameResultPresentationData& GameResultData,
		const FString& TitleMapName) const;
	void TravelToTitleWithoutGameResult(
		const FString& TitleMapName) const;

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	APdPlayerController* GetPdController() const;
	bool CanRequestExitMatchToTitle() const;
	void DestroySessionAndTravelToTitle(
		const FString& TitleMapName) const;
};
