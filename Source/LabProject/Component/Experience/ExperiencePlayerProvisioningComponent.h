#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Definition/Experience/ExperienceGameModeSettings.h"
#include "ExperiencePlayerProvisioningComponent.generated.h"

class APlayerController;
class UDefaultPlayerProvisioner;
class UExperiencePlayerProfileService;
struct FStreamableHandle;

/**
 * Coordinates server-side player provisioning in a stable, explicit order.
 *
 * DA_DefaultProvision grants are handled by one mode-driven provisioner.
 * Save/profile restoration remains separate because it is not a default grant.
 */
UCLASS(ClassGroup = (Experience))
class LABPROJECT_API UExperiencePlayerProvisioningComponent
	: public UActorComponent
{
	GENERATED_BODY()

public:
	UExperiencePlayerProvisioningComponent();

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
	void ApplyConfiguredStatusPointsForPlayerState(
		APlayerState* PlayerState);
	bool IsTrainingRoomMap() const;

private:
	struct FPendingGameplayProvision
	{
		TWeakObjectPtr<APlayerController> PlayerController;
		bool bApplyLobbySkinEquipment = false;
	};

	void BeginProvisioningContentPreload();
	void HandleProvisioningContentPreloaded();
	void ReleaseProvisioningContentPreload();
	void FlushPendingGameplayProvisions();
	void PreparePlayerForGameplayInternal(
		APlayerController* NewPlayer,
		bool bApplyLobbySkinEquipment);
	UPROPERTY(Transient)
	TObjectPtr<UExperiencePlayerProfileService>
		PlayerProfileService;

	UPROPERTY(Transient)
	TObjectPtr<UDefaultPlayerProvisioner>
		DefaultPlayerProvisioner;

	FExperiencePlayerProvisioningSettings CachedSettings;
	TArray<FPendingGameplayProvision> PendingGameplayProvisions;
	TSharedPtr<FStreamableHandle> ProvisioningContentLoadHandle;
	bool bProvisioningContentLoadPending = false;
	bool bProvisioningContentReady = false;
};
