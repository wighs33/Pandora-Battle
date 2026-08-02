#pragma once

#include "CoreMinimal.h"
#include "Mode/PdPlayerController.h"
#include "LobbyPlayerController.generated.h"

class ALobbyPlayerState;
class APawn;

UCLASS()
class LABPROJECT_API ALobbyPlayerController : public APdPlayerController
{
	GENERATED_BODY()

public:
	ALobbyPlayerController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;
	virtual void AcknowledgePossession(APawn* P) override;

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "!Lobby")
	void Server_HandleChangeNickname(const FText& InNickname);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "!Lobby")
	void Server_HandleChangeTeamColor(int32 InTeamColorIndex);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "!Lobby")
	void Server_HandleKickPlayer(ALobbyPlayerState* TargetPlayerState);

	UFUNCTION(Client, Reliable, Category = "!Lobby")
	void Client_KickedByHost(const FString& RoomTravelMapName);

	UFUNCTION(Client, Reliable, Category = "!Lobby|UI")
	void Client_RefreshLobbyUI();

	UFUNCTION(Client, Reliable, Category = "!Lobby|UI")
	void Client_StartGameCountdown(float DelaySeconds);

	UFUNCTION(Client, Reliable, Category = "!Lobby|UI")
	void Client_CancelGameStartCountdown();

	UFUNCTION(Client, Reliable, Category = "!Lobby|UI")
	void Client_ShowGameStartConnectingPopup();

	UFUNCTION(Client, Reliable, Category = "!Lobby|Travel")
	void Client_SetLobbyTravelLock(bool bLocked);

private:
	static FText SanitizeNickname(const FText& InNickname);
	void ApplyLobbyTravelLock(bool bLocked);
	void HandleDestroySessionForKick(bool bWasSuccessful);
	void TravelToPendingKickMap();

	bool bLobbyTravelLocked = false;
	FDelegateHandle DestroySessionCompleteHandle;
	FString PendingKickTravelMapName;
};
