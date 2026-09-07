#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "LobbyPlayerCoordinatorComponent.generated.h"

class AController;
class ALobbyGameMode;
class ALobbyPlayerState;
class APlayerController;
class APlayerState;

/**
 * 로비 참가자의 기본 이름·입장 슬롯을 배정하고 호스트의 강퇴 요청을 처리한다.
 */
UCLASS(ClassGroup = (Lobby))
class LABPROJECT_API ULobbyPlayerCoordinatorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULobbyPlayerCoordinatorComponent();

	void InitializeLobbyPlayerState(
		APlayerController* PlayerController,
		ALobbyPlayerState* LobbyPlayerState);
	void KickPlayer(ALobbyPlayerState* TargetPlayerState);
	APlayerController* ResolvePlayerControllerForPlayerState(
		const APlayerState* PlayerState) const;

private:
	ALobbyGameMode* GetLobbyGameMode() const;
	void AssignLobbySpawnIndexIfNeeded(
		ALobbyPlayerState* LobbyPlayerState) const;
	int32 FindAvailableLobbySpawnIndex(
		const ALobbyPlayerState* IgnoredPlayerState) const;
	void ForceKickPlayer(
		APlayerController* TargetPlayerController);

	int32 NicknameIndex = 0;
};
