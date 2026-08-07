#pragma once

#include "CoreMinimal.h"
#include "Definition/Match/MatchRuleDefinition.h"
#include "GameFramework/GameModeBase.h"
#include "LobbyGameMode.generated.h"

class AController;
class ALobbyPlayerState;
class APawn;
class ULobbyConfigurationComponent;
class ULobbyExperienceComponent;
class ULobbyMatchCoordinator;
class ULobbyModeDefinition;
class ULobbyPlayerCoordinatorComponent;
class UDefaultProvisionDefinition;
class UDefaultPlayerProvisioner;
class ULobbyRespawnComponent;
class ULobbyTravelCoordinator;
class UMatchRuleDefinition;

UCLASS()
class LABPROJECT_API ALobbyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ALobbyGameMode(
		const FObjectInitializer& ObjectInitializer =
			FObjectInitializer::Get());

	virtual void BeginPlay() override;
	virtual void EndPlay(
		const EEndPlayReason::Type EndPlayReason) override;
	virtual void InitGameState() override;
	virtual void PreLogin(
		const FString& Options,
		const FString& Address,
		const FUniqueNetIdRepl& UniqueId,
		FString& ErrorMessage) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual void HandleStartingNewPlayer_Implementation(
		APlayerController* NewPlayer) override;
	virtual UClass*
		GetDefaultPawnClassForController_Implementation(
			AController* InController) override;

	UFUNCTION(BlueprintCallable, Category = "!Lobby")
	void SaveConfig(
		FName MapKey,
		int32 InMaxPlayerCount,
		int32 InMaxBotCount);

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

	void RequestLobbyPlayerRespawn(
		AController* PlayerController,
		APawn* DeadPawn);
	void ProvisionLobbyPlayer(APlayerController* PlayerController);

	UFUNCTION(BlueprintPure, Category = "!Lobby")
	FString GetRoomTravelMapName() const;

	UFUNCTION(BlueprintPure, Category = "!Lobby")
	FString ResolveTravelMapName(FName MapKey) const;

	UFUNCTION(BlueprintPure, Category = "!Lobby")
	FName GetFirstMapKey() const;

	UFUNCTION(BlueprintPure, Category = "!Lobby|Map")
	int32 GetLobbyMapOptionCount() const;

	UFUNCTION(BlueprintPure, Category = "!Lobby|Map")
	bool GetLobbyMapOptionAtIndex(
		int32 Index,
		FLobbyMatchMapOption& OutMapOption) const;

	UFUNCTION(BlueprintPure, Category = "!Lobby|Map")
	bool GetSelectedLobbyMapOption(
		FLobbyMatchMapOption& OutMapOption) const;

	UFUNCTION(BlueprintPure, Category = "!Lobby|Map")
	FName GetSelectedLobbyMapKey() const;

	UFUNCTION(BlueprintPure, Category = "!Lobby|Map")
	int32 GetSelectedLobbyMaxPlayerCount() const;

	UFUNCTION(BlueprintCallable, Category = "!Lobby|Map")
	void SelectLobbyMapByOffset(int32 Offset);

	ULobbyConfigurationComponent*
		GetLobbyConfigurationComponent() const
	{
		return LobbyConfigurationComponent.Get();
	}
	ULobbyExperienceComponent*
		GetLobbyExperienceComponent() const
	{
		return LobbyExperienceComponent.Get();
	}
	ULobbyPlayerCoordinatorComponent*
		GetLobbyPlayerCoordinatorComponent() const
	{
		return LobbyPlayerCoordinatorComponent.Get();
	}
	ULobbyRespawnComponent*
		GetLobbyRespawnComponent() const
	{
		return LobbyRespawnComponent.Get();
	}
	ULobbyMatchCoordinator* GetMatchCoordinator() const
	{
		return MatchCoordinator.Get();
	}
	ULobbyTravelCoordinator* GetTravelCoordinator() const
	{
		return TravelCoordinator.Get();
	}

	const ULobbyModeDefinition*
		GetLobbyModeDefinition() const;
	const UMatchRuleDefinition*
		GetMatchRuleDefinition() const;
	const UDefaultProvisionDefinition*
		GetDefaultProvisionDefinition() const;

protected:
	void EnsureLobbyFrameworkClasses();

private:
	FName ResolveConfiguredMapKey(FName MapKey) const;
	bool FindConfiguredMapOption(
		FName MapKey,
		FLobbyMatchMapOption& OutMapOption) const;
	int32 GetConfiguredMaxPlayerCount() const;
	int32 GetConfiguredMaxBotCount(FName MapKey) const;
	ALobbyPlayerState* GetLobbyPlayerState(
		APlayerController* PlayerController) const;
	APlayerController*
		ResolvePlayerControllerForPlayerState(
			const APlayerState* PlayerState) const;
	void RefreshLobbyUIForAllPlayers();

	friend class ULobbyConfigurationComponent;
	friend class ULobbyExperienceComponent;
	friend class ULobbyMatchCoordinator;
	friend class ULobbyTravelCoordinator;

	UPROPERTY(VisibleAnywhere, Category = "!Lobby|Components",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULobbyConfigurationComponent>
		LobbyConfigurationComponent;

	UPROPERTY(VisibleAnywhere, Category = "!Lobby|Components",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULobbyExperienceComponent>
		LobbyExperienceComponent;

	UPROPERTY(VisibleAnywhere, Category = "!Lobby|Components",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULobbyPlayerCoordinatorComponent>
		LobbyPlayerCoordinatorComponent;

	UPROPERTY(VisibleAnywhere, Category = "!Lobby|Components",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULobbyRespawnComponent>
		LobbyRespawnComponent;

	UPROPERTY(Transient)
	TObjectPtr<ULobbyMatchCoordinator> MatchCoordinator;

	UPROPERTY(Transient)
	TObjectPtr<UDefaultPlayerProvisioner>
		DefaultPlayerProvisioner;

	UPROPERTY(Transient)
	TObjectPtr<ULobbyTravelCoordinator> TravelCoordinator;
};
