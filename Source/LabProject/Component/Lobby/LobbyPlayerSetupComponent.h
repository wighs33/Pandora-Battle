#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "LobbyPlayerSetupComponent.generated.h"

class AController;
class ALobbyGameMode;
class APdPlayerState;
class APlayerController;
class APlayerState;

/**
 * 로비 참가자의 기본 이름과 입장 슬롯을 준비한다.
 */
UCLASS(ClassGroup = (Lobby))
class LABPROJECT_API ULobbyPlayerSetupComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	ULobbyPlayerSetupComponent();

	void InitializeLobbyPlayerState(
		APlayerController* PlayerController,
		APdPlayerState* LobbyPlayerState);

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	ALobbyGameMode* GetLobbyGameMode() const;
	void AssignLobbySpawnIndexIfNeeded(
		APdPlayerState* LobbyPlayerState) const;
	int32 FindAvailableLobbySpawnIndex(
		const APdPlayerState* IgnoredPlayerState) const;

private:
	int32 NicknameIndex = 0;
};
