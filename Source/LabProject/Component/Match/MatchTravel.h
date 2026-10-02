#pragma once

#include "CoreMinimal.h"

class APlayerController;
class APlayerState;
class UWorld;
struct FGameResultPresentationData;

/** 경기가 끝난 참가자를 타이틀로 보낸다. 우리 PlayerController면 결과 표시 여부를 RPC로 함께 전한다. */
namespace MatchTravel
{
	/** 결과 없이 한 참가자만 타이틀로 보낸다. 자발적 퇴장으로 처리되어 연결 오류 팝업이 뜨지 않는다. */
	void SendPlayerToTitle(APlayerController& Player, const FString& TitleMapName);

	/** 결과를 이미 화면에 보여 줬을 때 모든 참가자를 결과 없이 타이틀로 보낸다. */
	void SendAllPlayersToTitle(UWorld& World, const FString& TitleMapName);

	/** 나간 플레이어를 뺀 참가자에게 결과를 들려 타이틀로 보낸다. 결과는 타이틀 화면에서 보여 준다. */
	void SendPlayersToTitleWithResult(
		UWorld& World,
		const FString& TitleMapName,
		const FGameResultPresentationData& GameResultData,
		const APlayerState* ExcludedPlayerState);
}
