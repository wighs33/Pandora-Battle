#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "UObject/PrimaryAssetId.h"
#include "LobbyGameMode.generated.h"

class APdPlayerState;
class ULobbyConfigurationComponent;
class UExperienceManagerComponent;
class UExperienceDefinition;
class ULobbyMatchCoordinator;
class ULobbyPlayerSetupComponent;
class UDefaultPlayerProvisioner;
class UPlayerSpawnComponent;
class ULobbyTravelCoordinator;

/**
 * 서버에서 로비 입장, 플레이어 시작과 게임 전환을 연결한다.
 *
 * 설정 조회와 세부 처리는 담당 컴포넌트 및 조정 객체가 맡고,
 * 엔진 초기화 순서와 로비의 주요 실행 명령은 여기서 관리한다.
 */
UCLASS()
class LABPROJECT_API ALobbyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void PreInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void InitGameState() override;
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
	void KickPlayer(APdPlayerState* TargetPlayerState);

	void ProvisionLobbyPlayer(APlayerController* PlayerController);
	bool IsReadyForPlayerStart() const;

	ULobbyConfigurationComponent* GetLobbyConfigurationComponent() const { return LobbyConfigurationComponent.Get(); }
	UPlayerSpawnComponent* GetSpawnComponent() const { return SpawnComponent; }
	ULobbyMatchCoordinator* GetMatchCoordinator() const { return MatchCoordinator.Get(); }
	ULobbyTravelCoordinator* GetTravelCoordinator() const { return TravelCoordinator.Get(); }

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	void ResumeWaitingPlayers();
	void HandleExperienceLoaded(const UExperienceDefinition* Experience);
	void HandleExperienceLoadFailed(FPrimaryAssetId ExperienceId, const FString& FailureMessage);
	void HandlePlayerRespawned(APlayerController* Player, bool bCreatedPawn);
	void StartExperienceLoad();
	FPrimaryAssetId GetConfiguredExperienceId() const;
	UExperienceManagerComponent* GetExperienceManager() const;

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void EnsureLobbyFrameworkClasses();

private:
	UPROPERTY(VisibleAnywhere, Category = "!Lobby|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULobbyConfigurationComponent> LobbyConfigurationComponent;

	UPROPERTY(VisibleAnywhere, Category = "!Lobby|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULobbyPlayerSetupComponent> LobbyPlayerSetupComponent;

	UPROPERTY(VisibleAnywhere, Category = "!Lobby|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPlayerSpawnComponent> SpawnComponent;

	UPROPERTY(Transient)
	TObjectPtr<ULobbyMatchCoordinator> MatchCoordinator;

	UPROPERTY(Transient)
	TObjectPtr<UDefaultPlayerProvisioner> DefaultPlayerProvisioner;

	UPROPERTY(Transient)
	TObjectPtr<ULobbyTravelCoordinator> TravelCoordinator;
};
