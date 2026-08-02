#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ExperienceLobbyProfileProvisioner.generated.h"

class AExperienceGameMode;
class APlayerController;
class UPdGameInstance;
struct FExperiencePlayerProvisioningSettings;

/**
 * Applies persisted lobby identity and cosmetic selections to a player.
 *
 * The owning provisioning component controls when this policy runs. This
 * object owns only lobby/profile-specific configuration and has no retries.
 */
UCLASS()
class LABPROJECT_API UExperienceLobbyProfileProvisioner : public UObject
{
	GENERATED_BODY()

public:
	void ApplySettings(
		const FExperiencePlayerProvisioningSettings& InSettings);
	void InitializeLoggedInPlayer(APlayerController* NewPlayer) const;
	void ApplyCachedLobbySkinEquipment(
		APlayerController* NewPlayer) const;

private:
	AExperienceGameMode* GetExperienceGameMode() const;
	void ApplyCachedLobbyPlayerIdentity(
		APlayerController* NewPlayer,
		UPdGameInstance& PdGameInstance) const;
	void ApplyInitialPlayerMapRegion(
		APlayerController* NewPlayer) const;

	bool bAssignDefaultTeamWhenLobbyTeamMissing = true;
	int32 DefaultLobbyTeamColorIndex = 0;
};
