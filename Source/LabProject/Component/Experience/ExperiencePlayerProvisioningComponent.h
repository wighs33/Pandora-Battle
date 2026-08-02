#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Definition/Experience/ExperienceGameModeSettings.h"
#include "ExperiencePlayerProvisioningComponent.generated.h"

class APlayerController;
class UExperienceGameplayLoadoutProvisioner;
class UExperienceLobbyProfileProvisioner;
class UExperienceTrainingRoomProvisioner;
struct FStreamableHandle;

/**
 * Coordinates server-side player provisioning in a stable, explicit order.
 *
 * Domain policy and retry state live in the lobby-profile, gameplay-loadout,
 * and training-room provisioners. This component remains the GameMode-facing
 * facade so existing lifecycle calls do not depend on those implementations.
 */
UCLASS(ClassGroup = (Experience))
class LABPROJECT_API UExperiencePlayerProvisioningComponent
	: public UActorComponent
{
	GENERATED_BODY()

public:
	UExperiencePlayerProvisioningComponent();

	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void ApplySettings(
		const FExperiencePlayerProvisioningSettings& InSettings);
	void InitializeLoggedInPlayer(APlayerController* NewPlayer);
	void PreparePlayerForGameplay(
		APlayerController* NewPlayer,
		bool bApplyLobbySkinEquipment);
	void ClearRuntimeStateForController(
		AController* Controller,
		APlayerState* PlayerState);
	void GrantTrainingRoomStatusPointsForPlayerState(
		APlayerState* PlayerState);

	bool IsTrainingRoomMap() const;
	int32 GetPendingDefaultItemGrantCount() const;

	const UExperienceLobbyProfileProvisioner*
	GetLobbyProfileProvisioner() const
	{
		return LobbyProfileProvisioner;
	}

	const UExperienceGameplayLoadoutProvisioner*
	GetGameplayLoadoutProvisioner() const
	{
		return GameplayLoadoutProvisioner;
	}

	const UExperienceTrainingRoomProvisioner*
	GetTrainingRoomProvisioner() const
	{
		return TrainingRoomProvisioner;
	}

private:
	struct FPendingGameplayProvision
	{
		TWeakObjectPtr<APlayerController> PlayerController;
		bool bApplyLobbySkinEquipment = false;
	};

	void EnsureRuntimeProvisioners();
	void ApplySettingsToProvisioners(
		const FExperiencePlayerProvisioningSettings& InSettings);
	void BeginProvisioningContentPreload();
	void HandleProvisioningContentPreloaded();
	void ReleaseProvisioningContentPreload();
	void FlushPendingGameplayProvisions();
	void PreparePlayerForGameplayInternal(
		APlayerController* NewPlayer,
		bool bApplyLobbySkinEquipment);

	UPROPERTY(VisibleAnywhere, Instanced, Category = "!Provisioning")
	TObjectPtr<UExperienceLobbyProfileProvisioner>
		LobbyProfileProvisioner;

	UPROPERTY(VisibleAnywhere, Instanced, Category = "!Provisioning")
	TObjectPtr<UExperienceGameplayLoadoutProvisioner>
		GameplayLoadoutProvisioner;

	UPROPERTY(VisibleAnywhere, Instanced, Category = "!Provisioning")
	TObjectPtr<UExperienceTrainingRoomProvisioner>
		TrainingRoomProvisioner;

	FExperiencePlayerProvisioningSettings CachedSettings;
	TArray<FPendingGameplayProvision> PendingGameplayProvisions;
	TSharedPtr<FStreamableHandle> ProvisioningContentLoadHandle;
	bool bProvisioningContentLoadPending = false;
	bool bProvisioningContentReady = false;
};
