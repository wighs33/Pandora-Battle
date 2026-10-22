#include "Component/Player/ControllerInputComponent.h"

#include "Component/Chat/ChatControllerComponent.h"
#include "Component/Player/ControllerPresentationComponent.h"
#include "Definition/Level/LevelDefinition.h"
#include "Interface/HudInputInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Mode/PdPlayerController.h"

// 화면 입력: 정보창·설정·로비·판도라 선택과 트리·점수판·채팅. 게임플레이 입력은 ControllerInputComponent.cpp에 있다.
// ESC는 CommonUI 뒤로 가기로 받는다(UGameplayScreen).

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
	if (IHudInputInterface* HUD = GetHudInput())
	{
		HUD->OnOpenSettingsMenuInputStarted(InputValue);
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
		if (UControllerPresentationComponent* Presentation = Controller->GetControllerPresentationComponent())
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
		if (UControllerPresentationComponent* Presentation = Controller->GetControllerPresentationComponent())
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
		if (UChatControllerComponent* ChatController = Controller->FindComponentByClass<UChatControllerComponent>())
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
		if (UChatControllerComponent* ChatController = Controller->FindComponentByClass<UChatControllerComponent>())
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

	const ULevelDefinition* Levels = ULevelDefinition::ResolveDefaultDefinition();
	const FString CurrentLevelName = UGameplayStatics::GetCurrentLevelName(this, true);
	return Levels && Levels->IsLobbyMapName(CurrentLevelName);
}

void UControllerInputComponent::ReleaseHeldUiInput()
{
	HandleSelectPandoraInputEnded(FInputActionValue());
	HandleScoreboardInputEnded(FInputActionValue());
}
