#pragma once

#include "CoreMinimal.h"
#include "Common/GameSessionConstants.h"
#include "Definition/Experience/ExperienceGameModeSettings.h"
#include "GameFramework/GameModeBase.h"
#include "UI/GameResultTypes.h"
#include "UObject/PrimaryAssetId.h"
#include "ExperienceGameMode.generated.h"

class UExperienceDefinition;
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

UCLASS(Blueprintable)
class LABPROJECT_API AExperienceGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AExperienceGameMode(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UExperienceMatchFlowComponent* GetMatchFlowComponent() const
	{
		return MatchFlowComponent.Get();
	}
	UExperienceSpawnComponent* GetSpawnComponent() const
	{
		return SpawnComponent.Get();
	}
	UExperiencePlayerProvisioningComponent* GetPlayerProvisioningComponent() const
	{
		return PlayerProvisioningComponent.Get();
	}

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void BeginPlay() override;
	virtual void InitGameState() override;
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;

	int32 GrantGameVictoryGoldReward(AController* WinnerController, int32 WinningTeamMemberCount = 1);

	void HandleMatchTimerExpired();

	bool ShowGameResultForWinner(APlayerState* WinnerPlayerState);

	void NotifyPlayerKillScored(APlayerState* KillerPlayerState, APlayerState* VictimPlayerState);

	bool RequestAbortMatchToTitle(APlayerController* RequestingPlayer);

	void RequestPlayerRespawn(AController* PlayerController, APawn* DeadPawn);

	bool TryGetPlayerInitialSpawnTransform(AController* PlayerController, FTransform& OutSpawnTransform) const;

	void ApplyConfiguredStatusPointsForPlayerState(APlayerState* PlayerState);

protected:
	bool IsExperienceLoaded() const;
	bool IsExperienceLoadPending() const;
	void StartExperienceLoad();
	void HandleExperienceLoaded(const UExperienceDefinition* Experience);
	void HandleExperienceLoadFailed(FPrimaryAssetId ExperienceId, const FString& FailureMessage);
	FPrimaryAssetId GetConfiguredExperienceId() const;

protected:
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
	void ApplyRuntimeComponentSettings();

	UPROPERTY(VisibleAnywhere, Category = "!Experience|Runtime")
	TObjectPtr<UExperienceMatchFlowComponent> MatchFlowComponent;

	UPROPERTY(VisibleAnywhere, Category = "!Experience|Runtime")
	TObjectPtr<UExperienceSpawnComponent> SpawnComponent;

	UPROPERTY(VisibleAnywhere, Category = "!Experience|Runtime")
	TObjectPtr<UExperiencePlayerProvisioningComponent>
		PlayerProvisioningComponent;
};
