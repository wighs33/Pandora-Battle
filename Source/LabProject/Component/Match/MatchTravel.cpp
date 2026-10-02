#include "Component/Match/MatchTravel.h"

#include "Common/GameResultTypes.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Mode/PdPlayerController.h"

void MatchTravel::SendPlayerToTitle(APlayerController& Player, const FString& TitleMapName)
{
	if (APdPlayerController* PdPlayerController = Cast<APdPlayerController>(&Player))
	{
		PdPlayerController->Client_TravelToTitleWithoutGameResult(TitleMapName);
	}
	else if (!TitleMapName.IsEmpty())
	{
		Player.ClientTravel(TitleMapName, TRAVEL_Absolute);
	}
}

void MatchTravel::SendAllPlayersToTitle(UWorld& World, const FString& TitleMapName)
{
	if (TitleMapName.IsEmpty())
	{
		return;
	}

	for (FConstPlayerControllerIterator Iterator = World.GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		if (APlayerController* PlayerController = Iterator->Get())
		{
			SendPlayerToTitle(*PlayerController, TitleMapName);
		}
	}
}

void MatchTravel::SendPlayersToTitleWithResult(
	UWorld& World,
	const FString& TitleMapName,
	const FGameResultPresentationData& GameResultData,
	const APlayerState* ExcludedPlayerState)
{
	if (TitleMapName.IsEmpty())
	{
		return;
	}

	for (FConstPlayerControllerIterator Iterator = World.GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* PlayerController = Iterator->Get();
		if (!PlayerController || PlayerController->PlayerState == ExcludedPlayerState)
		{
			continue;
		}

		if (APdPlayerController* PdPlayerController = Cast<APdPlayerController>(PlayerController))
		{
			PdPlayerController->Client_TravelToTitleWithGameResult(GameResultData, TitleMapName);
		}
		else
		{
			PlayerController->ClientTravel(TitleMapName, TRAVEL_Absolute);
		}
	}
}
