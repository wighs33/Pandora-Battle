#include "Component/Player/ControllerInputComponent.h"

#include "Component/Chat/ChatControllerComponent.h"
#include "Component/Player/ControllerPresentationComponent.h"
#include "Definition/Level/LevelDefinition.h"
#include "Definition/Player/ControllerInputDefinition.h"
#include "Interface/HudInputInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Mode/PdPlayerController.h"

// 화면 입력: 정보창·설정·Esc·로비·판도라 선택과 트리·점수판·채팅. 게임플레이 입력은 ControllerInputComponent.cpp에 있다.

void UControllerInputComponent::HandleOpenInfoInputStarted(const FInputActionValue& InputValue, const EInfoUiSection Section)
{
	static_cast<void>(InputValue);
	if (IHudInputInterface* HUD = GetHudInput())
	{
		HUD->OpenInfoUiFocused(Section);
	}
}

void UControllerInputComponent::HandleOpenSettingUiInputStarted(const FInputActionValue& InputValue)
{
	const UControllerInputDefinition* Definition = LoadedInputDefinition.Get();
	if (Definition
		&& Definition->GetOpenSettingUiInputAction().ToSoftObjectPath()
			== Definition->GetEscapeInputAction().ToSoftObjectPath())
	{
		// 예전 데이터 애셋은 두 칸 모두 IA_Escape를 가리킬 수 있다. 같은 키가 두 번 처리되지 않도록 Esc 처리에 맡긴다.
		return;
	}

	if (IHudInputInterface* HUD = GetHudInput())
	{
		HUD->OnOpenSettingsMenuInputStarted(InputValue);
	}
}

void UControllerInputComponent::HandleEscapeInputStarted(const FInputActionValue& InputValue)
{
	static_cast<void>(InputValue);

	APdPlayerController* Controller = GetPdController();
	if (Controller)
	{
		if (UChatControllerComponent* ChatController =
			Controller->FindComponentByClass<UChatControllerComponent>();
			ChatController && ChatController->IsChatFocused())
		{
			ChatController->ExitChat();
			return;
		}
	}

	if (IHudInputInterface* HUD = GetHudInput())
	{
		HUD->HandleEscapeInput();
		bSelectPandoraActionOpened = false;
	}
}

void UControllerInputComponent::HandleOpenLobbyInputStarted(const FInputActionValue& InputValue)
{
	static_cast<void>(InputValue);

	if (!IsOpenLobbyInputAllowed())
	{
		return;
	}

	if (IHudInputInterface* HUD = GetHudInput())
	{
		HUD->OpenLobbyUi();
	}
}

void UControllerInputComponent::HandleSelectPandoraInputStarted(const FInputActionValue& InputValue)
{
	static_cast<void>(InputValue);

	APdPlayer* PlayerCharacter = GetPlayerCharacter();
	if (!CanSwapPandoraAndWeapon(PlayerCharacter))
	{
		return;
	}

	if (IHudInputInterface* HUD = GetHudInput())
	{
		HUD->OnSelectPandoraInputStarted(InputValue);
		if (HUD->IsSelectPandoraUiOpen())
		{
			bSelectPandoraActionOpened = true;
		}
	}
}

void UControllerInputComponent::HandleSelectPandoraInputEnded(const FInputActionValue& InputValue)
{
	static_cast<void>(InputValue);

	if (!bSelectPandoraActionOpened)
	{
		return;
	}

	bSelectPandoraActionOpened = false;
	if (IHudInputInterface* HUD = GetHudInput())
	{
		HUD->OnSelectPandoraInputEnded(InputValue);
	}
}

void UControllerInputComponent::HandlePandoraTreeInputStarted(const FInputActionValue& InputValue)
{
	static_cast<void>(InputValue);

	if (IHudInputInterface* HUD = GetHudInput())
	{
		HUD->OnPandoraTreeInputStarted(InputValue);
	}
}

void UControllerInputComponent::HandleScoreboardInputStarted(const FInputActionValue& InputValue)
{
	static_cast<void>(InputValue);

	if (APdPlayerController* Controller = GetPdController())
	{
		if (UControllerPresentationComponent* Presentation =
			Controller->GetControllerPresentationComponent())
		{
			Presentation->ShowInGameScoreboard();
		}
	}
}

void UControllerInputComponent::HandleScoreboardInputEnded(const FInputActionValue& InputValue)
{
	static_cast<void>(InputValue);

	if (APdPlayerController* Controller = GetPdController())
	{
		if (UControllerPresentationComponent* Presentation =
			Controller->GetControllerPresentationComponent())
		{
			Presentation->HideInGameScoreboard();
		}
	}
}

void UControllerInputComponent::HandleChatInputStarted(const FInputActionValue& InputValue)
{
	static_cast<void>(InputValue);

	if (APdPlayerController* Controller = GetPdController())
	{
		if (UChatControllerComponent* ChatController =
			Controller->FindComponentByClass<UChatControllerComponent>())
		{
			ChatController->HandleChatInputAction();
		}
	}
}

void UControllerInputComponent::HandleChatScrollInputTriggered(const FInputActionValue& InputValue)
{
	const float ScrollValue = InputValue.Get<float>();
	if (FMath::IsNearlyZero(ScrollValue))
	{
		return;
	}

	if (APdPlayerController* Controller = GetPdController())
	{
		if (UChatControllerComponent* ChatController =
			Controller->FindComponentByClass<UChatControllerComponent>())
		{
			ChatController->ScrollChat(ScrollValue > 0.0f);
		}
	}
}

bool UControllerInputComponent::IsOpenLobbyInputAllowed() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const ULevelDefinition* Levels =
		ULevelDefinition::ResolveDefaultDefinition();
	const FString CurrentLevelName = UGameplayStatics::GetCurrentLevelName(this, true);
	return Levels
		&& Levels->IsLobbyMapName(CurrentLevelName);
}

void UControllerInputComponent::ReleaseHeldUiInput()
{
	HandleSelectPandoraInputEnded(FInputActionValue());
	HandleScoreboardInputEnded(FInputActionValue());
}
