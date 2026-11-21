#pragma once

#include "CoreMinimal.h"
#include "Mode/PdPlayerController.h"
#include "LobbyPlayerController.generated.h"

class APdPlayerState;
class APawn;

UCLASS()
class LABPROJECT_API ALobbyPlayerController : public APdPlayerController
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void AcknowledgePossession(APawn* P) override;

	// Public API ------------------------------------------------------------------------------------------------------
	ALobbyPlayerController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual bool UsesLobbyPresentation() const override { return true; }

	// Network RPCs ----------------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "!Lobby")
	void Server_HandleChangeNickname(const FText& InNickname);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "!Lobby")
	void Server_HandleChangeTeamColor(int32 InTeamColorIndex);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "!Lobby")
	void Server_HandleKickPlayer(APdPlayerState* TargetPlayerState);

	/** 방장: 빈 자리에 봇을 넣는다. */
	UFUNCTION(Server, Reliable, Category = "!Lobby|Bot")
	void Server_HandleAddBot();

	/** 방장: 봇의 팀 색을 바꾼다. */
	UFUNCTION(Server, Reliable, Category = "!Lobby|Bot")
	void Server_HandleChangeBotTeamColor(APdPlayerState* BotPlayerState, int32 InTeamColorIndex);

	/** 이 조종기가 방장(리슨 서버 호스트)인지. 봇 추가·강퇴처럼 방장만 하는 일을 거른다. */
	bool IsLobbyHost() const;

	UFUNCTION(Client, Reliable, Category = "!Lobby|UI")
	void Client_ShowGameStartConnectingPopup();

	UFUNCTION(Client, Reliable, Category = "!Lobby|UI")
	void Client_HideGameStartConnectingPopup();

	UFUNCTION(Client, Reliable, Category = "!Lobby|Travel")
	void Client_SetLobbyTravelLock(bool bLocked);

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	static FText SanitizeNickname(const FText& InNickname);
	void ApplyLobbyTravelLock(bool bLocked);

private:
	bool bLobbyTravelLocked = false;
};
