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
	virtual void BeginPlay() override;
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

	UFUNCTION(Client, Reliable, Category = "!Lobby|UI")
	void Client_StartGameCountdown(float DelaySeconds);

	UFUNCTION(Client, Reliable, Category = "!Lobby|UI")
	void Client_CancelGameStartCountdown();

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
