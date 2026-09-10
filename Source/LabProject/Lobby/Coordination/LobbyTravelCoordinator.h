#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "LobbyTravelCoordinator.generated.h"

class ALobbyGameMode;
class APdPlayerState;
class APlayerController;
class ULobbyRuntimeSubsystem;
enum class ELobbyContentPreloadResult : uint8;
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
	void HandleStartSessionComplete(bool bWasSuccessful);
	void ClearStartSessionDelegate();

	// 다음 전장에 전달할 맵·경기 옵션·플레이어 정보.
	void PrepareMatchTravel();
	bool ResolveSelectedMatchMap(FString& OutTravelMapName, FLobbyMatchMapOption& OutSelectedMapOption) const;
	FString BuildGameTravelUrl(const FString& TravelMapName) const;
	void PersistSelectedGameConfig(const FLobbyMatchMapOption& SelectedMapOption, const FString& TravelMapName) const;
	void CacheLobbyTravelState(ULobbyRuntimeSubsystem* LobbySubsystem) const;
	void CacheLobbyPlayerTravelState(ULobbyRuntimeSubsystem* LobbySubsystem, const APdPlayerState* LobbyPlayerState) const;
	TMap<FGameplayTag, FName> BuildEquippedSkinNamesBySlot(const APlayerController* PlayerController) const;

	// 콘텐츠 로딩과 클라이언트의 진입 화면을 준비한 후 서버 이동.
	void SetGameStartConnectingPopupVisible(bool bVisible) const;
	void PreloadContentAndScheduleTravel(const FString& TravelUrl);
	void CheckContentPreloadAndScheduleTravel();
	void HandleGameEntryContentPreloadFailure(ELobbyContentPreloadResult Result);
	void ScheduleServerTravel(const FString& TravelUrl);

	FDelegateHandle StartSessionCompleteHandle;
	FTimerHandle GameEntryContentPreloadPollTimerHandle;
	FTimerHandle TravelDelayTimerHandle;
	// 맵 경로와 NoMatchTimer 등의 옵션이 포함된 이동 URL.
	FString PendingTravelUrl;
};
