#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ExperiencePlayerProfileService.generated.h"

class AExperienceGameMode;
class APlayerController;
struct FExperiencePlayerProvisioningSettings;

/**
 * Applies persisted lobby identity and cosmetic selections to a player.
 *
 * This is a profile/session service, not a DA_DefaultProvision grant path.
 */
UCLASS(Transient)
class LABPROJECT_API UExperiencePlayerProfileService : public UObject
{
	GENERATED_BODY()

public:
	void ApplySettings(
		const FExperiencePlayerProvisioningSettings& InSettings);
	void InitializeLoggedInPlayer(APlayerController* NewPlayer) const;
	void InitializeMatchIdentity(APlayerController* NewPlayer) const;
	void ApplyCachedLobbySkinEquipment(
		APlayerController* NewPlayer) const;

private:
	AExperienceGameMode* GetExperienceGameMode() const;

	bool bAssignDefaultTeamWhenLobbyTeamMissing = true;
	int32 DefaultLobbyTeamColorIndex = 0;
};
