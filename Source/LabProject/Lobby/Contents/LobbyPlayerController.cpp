#include "Lobby/Contents/LobbyPlayerController.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Common/GameSessionConstants.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Lobby/Contents/LobbyGameMode.h"
#include "Lobby/Contents/LobbyHUD.h"
#include "Lobby/Contents/LobbyPlayerState.h"
#include "Lobby/LobbyRuntimeSubsystem.h"
#include "Lobby/UI/LobbyWidget.h"
#include "Mode/PdGameInstance.h"
#include "Component/Player/ControllerInputComponent.h"
#include "Settings/CursorSettingsLibrary.h"
#include "UI/UiSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LobbyPlayerController)

ALobbyPlayerController::ALobbyPlayerController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void ALobbyPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalController())
	{
		UWidgetBlueprintLibrary::SetInputMode_GameAndUIEx(this, nullptr, EMouseLockMode::DoNotLock, false, false);
		bShowMouseCursor = true;
		bEnableClickEvents = true;
		bEnableMouseOverEvents = true;
		UCursorSettingsLibrary::ApplyConfiguredMouseCursor(this, this);
		if (UPdGameInstance* PdGameInstance = GetGameInstance<UPdGameInstance>())
		{
			PdGameInstance->PlayBgmForContext(EBgmContext::Lobby);
		}
		if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
		{
			if (UUiSubsystem* UiSubsystem = LocalPlayer->GetSubsystem<UUiSubsystem>();
				UiSubsystem && UiSubsystem->IsTravelLoadingScreenActive())
			{
				UiSubsystem->ShowTravelLoadingScreen();
			}
		}
	}
}

void ALobbyPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
}

void ALobbyPlayerController::AcknowledgePossession(APawn* P)
{
	Super::AcknowledgePossession(P);

	if (UControllerInputComponent* ControllerInputComp = GetControllerInputComponent())
	{
		ControllerInputComp->RefreshInputDefinition();
	}
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

	ALobbyPlayerState* LobbyPlayerState = GetPlayerState<ALobbyPlayerState>();
	if (!LobbyPlayerState)
	{
		return;
	}

	if (InNickname.ToString().TrimStartAndEnd().IsEmpty())
	{
		LobbyPlayerState->ClearCustomNickname();
		return;
	}

	LobbyPlayerState->SetNickname(SanitizeNickname(InNickname));
}

void ALobbyPlayerController::Server_HandleChangeTeamColor_Implementation(const int32 InTeamColorIndex)
{
	if (!HasAuthority())
	{
		return;
	}

	ALobbyPlayerState* LobbyPlayerState = GetPlayerState<ALobbyPlayerState>();
	if (!LobbyPlayerState || LobbyPlayerState->IsLeavingLobby())
	{
		return;
	}

	const ALobbyGameMode* LobbyGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ALobbyGameMode>() : nullptr;
	const int32 MaxPlayerCount = LobbyGameMode ? LobbyGameMode->GetSelectedLobbyMaxPlayerCount() : LabGameSession::MaxPlayerCount;
	const int32 TeamColorIndex = FMath::Clamp(InTeamColorIndex, 0, FMath::Max(MaxPlayerCount, 1) - 1);
	if (LobbyPlayerState->GetTeamColorIndex() == TeamColorIndex)
	{
		return;
	}

LobbyPlayerState->SetTeamColorIndex(TeamColorIndex);

	if (ALobbyGameMode* MutableLobbyGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ALobbyGameMode>() : nullptr)
	{
		MutableLobbyGameMode->NotifyLobbyTeamChanged();
	}
}

void ALobbyPlayerController::Server_HandleKickPlayer_Implementation(ALobbyPlayerState* TargetPlayerState)
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

void ALobbyPlayerController::Client_RefreshLobbyUI_Implementation()
{
	if (ALobbyHUD* LobbyHUD = GetHUD<ALobbyHUD>())
	{
		LobbyHUD->RefreshLobbyUI();
	}
}

void ALobbyPlayerController::Client_StartGameCountdown_Implementation(const float DelaySeconds)
{
	ALobbyHUD* LobbyHUD = GetHUD<ALobbyHUD>();
	if (LobbyHUD
		&& (!IsValid(LobbyHUD->GetLobbyWidget()) || !LobbyHUD->GetLobbyWidget()->IsInViewport()))
	{
		LobbyHUD->CreateLobbyUI();
	}

	ULobbyWidget* LobbyWidget = LobbyHUD ? LobbyHUD->GetLobbyWidget() : nullptr;
	if (!LobbyWidget)
	{

		return;
	}

	LobbyWidget->StartGameCountdown(DelaySeconds);
}

void ALobbyPlayerController::Client_CancelGameStartCountdown_Implementation()
{
	ALobbyHUD* LobbyHUD = GetHUD<ALobbyHUD>();
	if (LobbyHUD && !LobbyHUD->GetLobbyWidget())
	{
		LobbyHUD->CreateLobbyUI();
	}

	ULobbyWidget* LobbyWidget = LobbyHUD ? LobbyHUD->GetLobbyWidget() : nullptr;
	if (!LobbyWidget)
	{

		return;
	}

	LobbyWidget->HideGameCountdown();

}

void ALobbyPlayerController::Client_ShowGameStartConnectingPopup_Implementation()
{
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UUiSubsystem* UiSubsystem = LocalPlayer ? LocalPlayer->GetSubsystem<UUiSubsystem>() : nullptr;
	if (!UiSubsystem)
	{

		return;
	}

	UiSubsystem->ShowTravelLoadingScreen();
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (ULobbyRuntimeSubsystem* LobbyRuntimeSubsystem =
			GameInstance->GetSubsystem<ULobbyRuntimeSubsystem>())
		{
			LobbyRuntimeSubsystem->BeginGameEntryContentPreload();
		}
	}

}

void ALobbyPlayerController::Client_HideGameStartConnectingPopup_Implementation()
{
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (UUiSubsystem* UiSubsystem =
		LocalPlayer ? LocalPlayer->GetSubsystem<UUiSubsystem>() : nullptr)
	{
		UiSubsystem->HideTravelLoadingScreen();
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
	MovementComponent->SetBase(nullptr);

	if (bLocked)
	{
		MovementComponent->DisableMovement();
		return;
	}

	MovementComponent->SetMovementMode(MOVE_Walking);
}
