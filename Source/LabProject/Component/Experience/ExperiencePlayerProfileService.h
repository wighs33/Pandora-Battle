#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ExperiencePlayerProfileService.generated.h"

class AExperienceGameMode;
class APlayerController;
struct FExperiencePlayerProvisioningSettings;

/**
 * 저장된 로비 식별 정보와 외형 선택을 플레이어에게 적용한다.
 *
 * 프로필·세션 처리를 담당하며, DA_DefaultProvision의 기본 지급 경로와는 별개다.
 */
UCLASS(Transient)
class LABPROJECT_API UExperiencePlayerProfileService : public UObject
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	void ApplySettings(
		const FExperiencePlayerProvisioningSettings& InSettings);
	void InitializeLoggedInPlayer(APlayerController* NewPlayer) const;
	void InitializeMatchIdentity(APlayerController* NewPlayer) const;
	void ApplyCachedLobbySkinEquipment(
		APlayerController* NewPlayer) const;

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	AExperienceGameMode* GetExperienceGameMode() const;

private:
	bool bAssignDefaultTeamWhenLobbyTeamMissing = true;
	int32 DefaultLobbyTeamColorIndex = 0;
};
