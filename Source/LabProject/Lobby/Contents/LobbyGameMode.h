#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "LobbyGameMode.generated.h"

class ALobbyPlayerState;
class ULobbyConfigurationComponent;
class ULobbyExperienceComponent;
class ULobbyMatchCoordinator;
class ULobbyPlayerCoordinatorComponent;
class UDefaultPlayerProvisioner;
class ULobbyRespawnComponent;
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
	ALobbyGameMode(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//------------------------------------------------------------------------------------------------------------------
	//--- Engine Callbacks
	virtual void PreInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void InitGameState() override;
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual void GenericPlayerInitialization(AController* Controller) override;
	virtual void Logout(AController* Exiting) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

	//------------------------------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "!Lobby")
	void SaveConfig(FName MapKey, int32 InMaxPlayerCount, int32 InMaxBotCount);

	UFUNCTION(BlueprintCallable, Category = "!Lobby")
	void TryStartGame();

	UFUNCTION(BlueprintPure, Category = "!Lobby")
	bool CanHostStartGame() const;

	UFUNCTION(BlueprintPure, Category = "!Lobby|Team")
	bool AreLobbyTeamsBalanced() const;

	UFUNCTION(BlueprintCallable, Category = "!Lobby|Team")
	void NotifyLobbyTeamChanged();

	UFUNCTION(BlueprintCallable, Category = "!Lobby")
	void KickPlayer(ALobbyPlayerState* TargetPlayerState);

	void RequestLobbyPlayerRespawn(AController* PlayerController, APawn* DeadPawn);
	void ProvisionLobbyPlayer(APlayerController* PlayerController);

	ULobbyConfigurationComponent* GetLobbyConfigurationComponent() const { return LobbyConfigurationComponent.Get(); }
	ULobbyExperienceComponent* GetLobbyExperienceComponent() const { return LobbyExperienceComponent.Get(); }
	ULobbyPlayerCoordinatorComponent* GetLobbyPlayerCoordinatorComponent() const { return LobbyPlayerCoordinatorComponent.Get(); }
	ULobbyRespawnComponent* GetLobbyRespawnComponent() const { return LobbyRespawnComponent.Get(); }
	ULobbyMatchCoordinator* GetMatchCoordinator() const { return MatchCoordinator.Get(); }
	ULobbyTravelCoordinator* GetTravelCoordinator() const { return TravelCoordinator.Get(); }

private:
	void EnsureLobbyFrameworkClasses();

	//------------------------------------------------------------------------------------------------------------------
	//--- Components
	UPROPERTY(VisibleAnywhere, Category = "!Lobby|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULobbyConfigurationComponent> LobbyConfigurationComponent;

	UPROPERTY(VisibleAnywhere, Category = "!Lobby|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULobbyExperienceComponent> LobbyExperienceComponent;

	UPROPERTY(VisibleAnywhere, Category = "!Lobby|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULobbyPlayerCoordinatorComponent> LobbyPlayerCoordinatorComponent;

	UPROPERTY(VisibleAnywhere, Category = "!Lobby|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULobbyRespawnComponent> LobbyRespawnComponent;

	//------------------------------------------------------------------------------------------------------------------
	UPROPERTY(Transient)
	TObjectPtr<ULobbyMatchCoordinator> MatchCoordinator;

	UPROPERTY(Transient)
	TObjectPtr<UDefaultPlayerProvisioner> DefaultPlayerProvisioner;

	UPROPERTY(Transient)
	TObjectPtr<ULobbyTravelCoordinator> TravelCoordinator;
};
