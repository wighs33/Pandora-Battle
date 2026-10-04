#include "Lobby/Contents/LobbyPlayerController.h"
#include "Component/Lobby/LobbyPlayerStateComponent.h"
#include "Component/Player/PlayerMatchComponent.h"

#include "Component/Lobby/LobbyConfigurationComponent.h"
#include "Common/GameSessionConstants.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Lobby/Contents/LobbyGameMode.h"
#include "Mode/PdPlayerState.h"
#include "Lobby/LobbyRuntimeSubsystem.h"
#include "Engine/GameInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LobbyPlayerController)

ALobbyPlayerController::ALobbyPlayerController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void ALobbyPlayerController::AcknowledgePossession(APawn* P)
{
	Super::AcknowledgePossession(P);

	if (bLobbyTravelLocked)
	{
		ApplyLobbyTravelLock(true);
	}
}

void ALobbyPlayerController::Server_HandleChangeNickname_Implementation(const FText& InNickname)
{
	if (!HasAuthority())
	{
		return;
	}

	APdPlayerState* LobbyPlayerState = GetPlayerState<APdPlayerState>();
	if (!LobbyPlayerState)
	{
		return;
	}

	if (InNickname.ToString().TrimStartAndEnd().IsEmpty())
	{
		LobbyPlayerState->GetLobbyPlayerStateComponent()->ClearCustomNickname();
		return;
	}

	LobbyPlayerState->GetLobbyPlayerStateComponent()->SetNickname(SanitizeNickname(InNickname));
}

void ALobbyPlayerController::Server_HandleChangeTeamColor_Implementation(const int32 InTeamColorIndex)
{
	if (!HasAuthority())
	{
		return;
	}

	APdPlayerState* LobbyPlayerState = GetPlayerState<APdPlayerState>();
	if (!LobbyPlayerState || LobbyPlayerState->GetLobbyPlayerStateComponent()->IsLeavingLobby())
	{
		return;
	}

	const ALobbyGameMode* LobbyGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ALobbyGameMode>() : nullptr;
	const int32 MaxPlayerCount = LobbyGameMode ? LobbyGameMode->GetLobbyConfigurationComponent()->GetConfiguredMaxPlayerCount() : LabGameSession::MaxPlayerCount;
	const int32 TeamColorIndex = FMath::Clamp(InTeamColorIndex, 0, FMath::Max(MaxPlayerCount, 1) - 1);
	if (LobbyPlayerState->GetPlayerMatchComponent()->GetMatchTeamColorIndex() == TeamColorIndex)
	{
		return;
	}

	LobbyPlayerState->GetPlayerMatchComponent()->SetMatchTeamColorIndex(TeamColorIndex);

	if (ALobbyGameMode* MutableLobbyGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ALobbyGameMode>() : nullptr)
	{
		MutableLobbyGameMode->NotifyLobbyTeamChanged();
	}
}

void ALobbyPlayerController::Server_HandleKickPlayer_Implementation(APdPlayerState* TargetPlayerState)
{
	if (!HasAuthority() || !TargetPlayerState)
	{
		return;
	}

	if (!IsLocalController())
	{
		return;
	}

	if (TargetPlayerState == PlayerState)
	{
		return;
	}

	if (ALobbyGameMode* LobbyGameMode = Cast<ALobbyGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		LobbyGameMode->KickPlayer(TargetPlayerState);
	}
}

void ALobbyPlayerController::Client_ShowGameStartConnectingPopup_Implementation()
{
	UGameInstance* GameInstance = GetGameInstance();
	if (ULobbyRuntimeSubsystem* LobbyRuntimeSubsystem =
		GameInstance ? GameInstance->GetSubsystem<ULobbyRuntimeSubsystem>() : nullptr)
	{
		LobbyRuntimeSubsystem->SetGameStartPreparationPending(true);
		LobbyRuntimeSubsystem->BeginGameEntryContentPreload();
	}
}

void ALobbyPlayerController::Client_HideGameStartConnectingPopup_Implementation()
{
	UGameInstance* GameInstance = GetGameInstance();
	if (ULobbyRuntimeSubsystem* LobbyRuntimeSubsystem =
		GameInstance ? GameInstance->GetSubsystem<ULobbyRuntimeSubsystem>() : nullptr)
	{
		LobbyRuntimeSubsystem->SetGameStartPreparationPending(false);
	}
}

void ALobbyPlayerController::Client_SetLobbyTravelLock_Implementation(const bool bLocked)
{
	ApplyLobbyTravelLock(bLocked);
}

FText ALobbyPlayerController::SanitizeNickname(const FText& InNickname)
{
	FString NicknameString = InNickname.ToString().TrimStartAndEnd();
	if (NicknameString.IsEmpty())
	{
		NicknameString = TEXT("Player");
	}

	constexpr int32 MaxNicknameLength = 16;
	if (NicknameString.Len() > MaxNicknameLength)
	{
		NicknameString.LeftInline(MaxNicknameLength);
	}

	return FText::FromString(NicknameString);
}

void ALobbyPlayerController::ApplyLobbyTravelLock(const bool bLocked)
{
	if (bLobbyTravelLocked != bLocked)
	{
		SetIgnoreMoveInput(bLocked);
		bLobbyTravelLocked = bLocked;
	}

	ACharacter* ControlledCharacter = Cast<ACharacter>(GetPawn());
	UCharacterMovementComponent* MovementComponent = ControlledCharacter ? ControlledCharacter->GetCharacterMovement() : nullptr;
	if (!MovementComponent)
	{
		return;
	}

	MovementComponent->StopMovementImmediately();
	MovementComponent->SetBase(static_cast<FMovementBaseInterfaceData*>(nullptr));

	if (bLocked)
	{
		MovementComponent->DisableMovement();
		return;
	}

	MovementComponent->SetMovementMode(MOVE_Walking);
}
