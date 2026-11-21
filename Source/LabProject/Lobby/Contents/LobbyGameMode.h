#pragma once

#include "CoreMinimal.h"
#include "Experience/ExperienceHostGameMode.h"
#include "GameFramework/GameModeBase.h"
#include "UObject/PrimaryAssetId.h"
#include "LobbyGameMode.generated.h"

class APdBotController;
class APdPlayerState;
class ULobbyConfigurationComponent;
class UExperienceManagerComponent;
class UExperienceDefinition;
class ULobbyPlayerSetupComponent;
class UDefaultPlayerProvisioner;
class UPlayerSpawnComponent;
class ULobbyTravelCoordinator;
struct FStreamableHandle;

/**
 * 로비 입장·팀 배정·경기 시작 조건과 카운트다운을 소유한다.
 * 설정·플레이어 준비·스폰은 각 컴포넌트, 비동기 전장 이동은 TravelCoordinator가 담당한다.
 */
UCLASS()
class LABPROJECT_API ALobbyGameMode : public AGameModeBase, public IExperienceHostGameMode
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
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void GenericPlayerInitialization(AController* Controller) override;
	virtual void Logout(AController* Exiting) override;
	virtual void PostSeamlessTravel() override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

	// Public API ------------------------------------------------------------------------------------------------------
	ALobbyGameMode(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// 외부에서는 로비 명령과 시작 조건만 사용하고, 담당 컴포넌트 선택은 GameMode에 맡긴다.
	void SelectLobbyMap(FName MapKey);
	void SelectLobbyMapByOffset(int32 Offset);
	void TryStartGame();
	bool CanHostStartGame() const;
	void NotifyLobbyTeamChanged();
	void CancelPendingGameStart();
	void KickPlayer(APdPlayerState* TargetPlayerState);

	/**
	 * 방장: 빈 자리에 봇을 하나 넣는다. 봇은 사람 참가자처럼 PlayerState를 가지고 인원·팀·시작 조건에 함께 센다.
	 * 정원이 찼거나 경기 시작을 기다리는 중이면 넣지 않는다.
	 */
	bool AddBot();
	/** 정원이 남아 방장이 봇을 넣을 수 있는지. */
	bool CanAddBot() const;
	/** 봇을 뺀다. 봇 조종기를 지우면 엔진이 퇴장(Logout)과 PlayerState 정리를 한다. */
	void RemoveBot(APdPlayerState* BotPlayerState);
	/** 방장: 봇의 팀 색을 바꾼다(사람은 자기 팀만 바꾼다). */
	void SetBotTeamColor(APdPlayerState* BotPlayerState, int32 TeamColorIndex);
	/** 대기실을 닫고 타이틀로 갈 때 봇을 모두 뺀다. 봇 조종기는 PlayerState가 있어 심리스 이동을 따라가기 때문이다. */
	void RemoveAllBots();

	void ProvisionLobbyPlayer(AController* Player);
	bool IsReadyForPlayerStart() const;

	ULobbyConfigurationComponent* GetLobbyConfigurationComponent() const { return LobbyConfigurationComponent.Get(); }
	UPlayerSpawnComponent* GetSpawnComponent() const { return SpawnComponent; }

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	void HandleStartCountdownElapsed();
	void ResumeWaitingPlayers();
	void HandleExperienceLoaded(const UExperienceDefinition* Experience);
	void HandleExperienceLoadFailed(FPrimaryAssetId ExperienceId, const FString& FailureMessage);
	void HandlePlayerRespawned(AController* Player, bool bCreatedPawn);
	void UpdateDedicatedServerAutoStart();
	void StartExperienceLoad();
	virtual FPrimaryAssetId GetConfiguredExperienceId() const override;
	UExperienceManagerComponent* GetExperienceManager() const;

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	bool AreMatchStartConditionsMet() const;
	int32 GetActiveLobbyPlayerCount() const;
	/** 접속 정원은 사람만 센다. 사람이 들어와 정원을 넘으면 봇이 자리를 비켜 준다. */
	int32 GetHumanPlayerCount(const APlayerState* IgnoredPlayerState = nullptr) const;
	/** 대기실의 봇들(넣은 순서). */
	TArray<APdPlayerState*> GetBotPlayerStates() const;
	/** 정원보다 참가자가 많으면 마지막에 넣은 봇부터 뺀다. */
	void RemoveBotsOverCapacity();
	void RemoveBotsWithoutHumans();
	void PreloadBotPandoras();
	FText MakeBotDisplayName() const;
	bool AreLobbyTeamsBalanced() const;
	float GetStartCountdownSeconds() const;
	void AssignLobbyTeamColorIfNeeded(APdPlayerState* PlayerState) const;
	int32 FindAvailableLobbyTeamColorIndex(const APdPlayerState* IgnoredPlayerState) const;
	void UpdateAdvertisedSessionSettings() const;

	// 비동기 이동 직전에도 GameMode의 시작 조건으로 재검증한다.
	friend class ULobbyTravelCoordinator;
	FTimerHandle StartCountdownTimerHandle;
	bool bGameStartRequested = false;

	// 전용 서버에는 시작 버튼을 누를 호스트가 없으므로 인원·팀 조건이 유지되면 자동으로 시작한다.
	FTimerHandle DedicatedServerAutoStartTimerHandle;
	double DedicatedServerAutoStartReadyTimeSeconds = -1.0;

	UPROPERTY(VisibleAnywhere, Category = "!Lobby|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULobbyConfigurationComponent> LobbyConfigurationComponent;

	UPROPERTY(VisibleAnywhere, Category = "!Lobby|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULobbyPlayerSetupComponent> LobbyPlayerSetupComponent;

	UPROPERTY(VisibleAnywhere, Category = "!Lobby|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPlayerSpawnComponent> SpawnComponent;

	UPROPERTY(Transient, Instanced)
	TObjectPtr<UDefaultPlayerProvisioner> DefaultPlayerProvisioner;

	UPROPERTY(Transient, Instanced)
	TObjectPtr<ULobbyTravelCoordinator> TravelCoordinator;

	/** 봇이 경기에서 쓸 판도라를 고를 수 있게 판도라 정의를 붙잡아 둔다. 처음 봇을 넣을 때 불러온다. */
	TSharedPtr<FStreamableHandle> BotPandoraPreloadHandle;

	/** 방장이 넣는 봇의 조종기. */
	UPROPERTY(EditDefaultsOnly, Category = "!Lobby|Bot")
	TSubclassOf<APdBotController> BotControllerClass;
};
