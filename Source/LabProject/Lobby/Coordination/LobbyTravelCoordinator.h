#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "LobbyTravelCoordinator.generated.h"

class ALobbyGameMode;
class ALobbyPlayerState;
class APlayerController;
class UPdGameInstance;
struct FLobbyMatchMapOption;

UCLASS(Transient)
class LABPROJECT_API ULobbyTravelCoordinator : public UObject
{
	GENERATED_BODY()

public:
	void StartSessionAndTravel();
	void CancelPendingTravel();
	void Shutdown();

	void SetAllLobbyPawnsTravelLocked(bool bLocked) const;
	void SetLobbyPawnTravelLocked(APlayerController* PlayerController, bool bLocked) const;

private:
	ALobbyGameMode* GetLobbyGameMode() const;
	bool IsLobbyReadyForSelectedMap() const;
	void HandleStartSessionComplete(bool bWasSuccessful);
	void ClearStartSessionDelegate();
	void StartGameTravel();
	bool ResolveSelectedGameTravel(
		FString& OutTravelMapName,
		FLobbyMatchMapOption& OutSelectedMapOption) const;
	FString BuildGameTravelUrl(const FString& TravelMapName) const;
	bool ShouldStartGameWithoutMatchTimer() const;
	void PersistSelectedGameConfig(
		const FLobbyMatchMapOption& SelectedMapOption,
		const FString& TravelMapName) const;
	void CacheLobbyTravelState(UPdGameInstance* PdGameInstance) const;
	void CacheLobbyPlayerTravelState(
		UPdGameInstance* PdGameInstance,
		const ALobbyPlayerState* LobbyPlayerState) const;
	TMap<FGameplayTag, FName> BuildEquippedSkinNamesBySlot(
		const APlayerController* PlayerController) const;
	void ShowGameStartConnectingPopupForAllPlayers() const;
	void ScheduleServerTravel(const FString& TravelMapName);

	FDelegateHandle StartSessionCompleteHandle;
	FTimerHandle TravelDelayTimerHandle;
};
