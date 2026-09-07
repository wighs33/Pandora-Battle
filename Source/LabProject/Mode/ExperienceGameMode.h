#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "UObject/PrimaryAssetId.h"
#include "ExperienceGameMode.generated.h"

class UExperienceDefinition;
class UExperienceManagerComponent;
class UExperienceMatchFlowComponent;
class UExperiencePlayerProvisioningComponent;
class UExperienceSpawnComponent;
class UMatchRuleDefinition;
class URewardDefinition;
class UWorld;
class APawn;
class APlayerController;
class APlayerState;

DECLARE_LOG_CATEGORY_EXTERN(PdExperienceGameModeLog, Log, All);

/**
 * 서버의 입장·스폰·경기 시작 조건을 연결한다.
 *
 * 경기 진행, 스폰 배정, 플레이어 지급의 실행 상태는 각 전용 컴포넌트가 관리한다.
 */
UCLASS(Blueprintable)
class LABPROJECT_API AExperienceGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AExperienceGameMode(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//------------------------------------------------------------------------------------------------------------------
	//--- Engine Callbacks
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void InitGameState() override;
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual void GenericPlayerInitialization(AController* Controller) override;
	virtual void OnPostLogin(AController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;

	//------------------------------------------------------------------------------------------------------------------

	UExperienceMatchFlowComponent* GetMatchFlowComponent() const { return MatchFlowComponent; }
	UExperienceSpawnComponent* GetSpawnComponent() const { return SpawnComponent; }
	UExperiencePlayerProvisioningComponent* GetPlayerProvisioningComponent() const { return PlayerProvisioningComponent; }

	void NotifyPlayerKillScored(APlayerState* KillerPlayerState, APlayerState* VictimPlayerState);
	bool RequestAbortMatchToTitle(APlayerController* RequestingPlayer);
	void RequestPlayerRespawn(AController* PlayerController, APawn* DeadPawn);
	bool TryGetPlayerInitialSpawnTransform(AController* PlayerController, FTransform& OutSpawnTransform) const;
	void ApplyConfiguredStatusPointsForPlayerState(APlayerState* PlayerState);

protected:
	bool CanStartGameplay() const;
	void StartExperienceLoad();
	void HandleExperienceLoaded(const UExperienceDefinition* Experience);
	void HandleExperienceLoadFailed(FPrimaryAssetId ExperienceId, const FString& FailureMessage);
	FPrimaryAssetId GetConfiguredExperienceId() const;

	// Experience 미지정과 로딩 실패는 구분한다. 선택 기능인 맵만 실패 후 기본 Pawn 실행을 허용한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Experience")
	bool bAllowNativePawnOnExperienceLoadFailure = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Spawn")
	bool bUseLobbySpawnIndexPlayerStarts = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Spawn")
	FName LobbySpawnPlayerStartTagPrefix = TEXT("Spawn_");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Reward|Gold")
	TSoftObjectPtr<URewardDefinition> GameVictoryRewardDefinition;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Reward|Gold", meta = (ClampMin = "0"))
	int32 VictoryGoldPerKill = 100;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Reward|Gold", meta = (ClampMin = "0"))
	int32 VictoryGoldPenaltyPerDeath = 50;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Reward|Gold", meta = (ClampMin = "0"))
	int32 VictoryGoldPerWinningTeamMember = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Reward|Chest Spawn",
		meta = (ToolTip = "Reward definition that controls how many placed reward chests stay active at match start. If unset, the first placed chest with a RewardDefinition is used."))
	TSoftObjectPtr<URewardDefinition> ChestSpawnRewardDefinition;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Match Rules")
	TSoftObjectPtr<UMatchRuleDefinition> MatchRuleDefinition;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Team")
	bool bAssignDefaultTeamWhenLobbyTeamMissing = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Team", meta = (ClampMin = "0"))
	int32 DefaultLobbyTeamColorIndex = 0;

private:
	UExperienceManagerComponent* GetExperienceManager() const;
	void ResumeStartingPlayers();
	void TryStartServerMatch();
	void ApplyRuntimeComponentSettings();
	bool bExperienceLoadFailed = false;

	//------------------------------------------------------------------------------------------------------------------
	//--- Components
	UPROPERTY(VisibleAnywhere, Category = "!Experience|Runtime")
	TObjectPtr<UExperienceMatchFlowComponent> MatchFlowComponent;

	UPROPERTY(VisibleAnywhere, Category = "!Experience|Runtime")
	TObjectPtr<UExperienceSpawnComponent> SpawnComponent;

	UPROPERTY(VisibleAnywhere, Category = "!Experience|Runtime")
	TObjectPtr<UExperiencePlayerProvisioningComponent> PlayerProvisioningComponent;
};
