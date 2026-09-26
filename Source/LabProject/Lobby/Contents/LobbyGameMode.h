#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "UObject/PrimaryAssetId.h"
#include "LobbyGameMode.generated.h"

class APdPlayerState;
class ULobbyConfigurationComponent;
class UExperienceManagerComponent;
class UExperienceDefinition;
class ULobbyPlayerSetupComponent;
class UDefaultPlayerProvisioner;
class UPlayerSpawnComponent;
class ULobbyTravelCoordinator;

/**
 * 로비 입장·팀 배정·경기 시작 조건과 카운트다운을 소유한다.
 * 설정·플레이어 준비·스폰은 각 컴포넌트, 비동기 전장 이동은 TravelCoordinator가 담당한다.
 */
UCLASS()
class LABPROJECT_API ALobbyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void InitGameState() override;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* Player, const FTransform& Transform) override;
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual void GenericPlayerInitialization(AController* Controller) override;
	virtual void Logout(AController* Exiting) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

	// Public API ------------------------------------------------------------------------------------------------------
	ALobbyGameMode(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// 외부에서는 로비 명령과 시작 조건만 사용하고, 담당 컴포넌트 선택은 GameMode에 맡긴다.
	void SaveConfig(FName MapKey, int32 InMaxBotCount);
	void SelectLobbyMapByOffset(int32 Offset);
	void TryStartGame();
	bool CanHostStartGame() const;
	void NotifyLobbyTeamChanged();
	void CancelPendingGameStart();
	void KickPlayer(APdPlayerState* TargetPlayerState);

	void ProvisionLobbyPlayer(APlayerController* PlayerController);
	bool IsReadyForPlayerStart() const;

	ULobbyConfigurationComponent* GetLobbyConfigurationComponent() const { return LobbyConfigurationComponent.Get(); }
	UPlayerSpawnComponent* GetSpawnComponent() const { return SpawnComponent; }

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	void HandleStartCountdownElapsed();
	void ResumeWaitingPlayers();
	void HandleExperienceLoaded(const UExperienceDefinition* Experience);
	void HandleExperienceLoadFailed(FPrimaryAssetId ExperienceId, const FString& FailureMessage);
	void HandlePlayerRespawned(APlayerController* Player, bool bCreatedPawn);
	void StartExperienceLoad();
	FPrimaryAssetId GetConfiguredExperienceId() const;
	UExperienceManagerComponent* GetExperienceManager() const;

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	bool AreMatchStartConditionsMet() const;
	int32 GetActiveLobbyPlayerCount() const;
	bool AreLobbyTeamsBalanced() const;
	float GetStartCountdownSeconds() const;
	void AssignLobbyTeamColorIfNeeded(APdPlayerState* PlayerState) const;
	int32 FindAvailableLobbyTeamColorIndex(const APdPlayerState* IgnoredPlayerState) const;
	void UpdateAdvertisedSessionSettings() const;

	// 비동기 이동 직전에도 GameMode의 시작 조건으로 재검증한다.
	friend class ULobbyTravelCoordinator;
	FTimerHandle StartCountdownTimerHandle;
	bool bGameStartRequested = false;

	UPROPERTY(VisibleAnywhere, Category = "!Lobby|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULobbyConfigurationComponent> LobbyConfigurationComponent;

	UPROPERTY(VisibleAnywhere, Category = "!Lobby|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULobbyPlayerSetupComponent> LobbyPlayerSetupComponent;

	UPROPERTY(VisibleAnywhere, Category = "!Lobby|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPlayerSpawnComponent> SpawnComponent;

	UPROPERTY(Transient)
	TObjectPtr<UDefaultPlayerProvisioner> DefaultPlayerProvisioner;

	UPROPERTY(Transient)
	TObjectPtr<ULobbyTravelCoordinator> TravelCoordinator;
};
