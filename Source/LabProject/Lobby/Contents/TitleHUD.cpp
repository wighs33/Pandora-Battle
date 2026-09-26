#include "Lobby/Contents/TitleHUD.h"
#include "UI/UiSubsystem.h"
#include "UI/UiScreen.h"
#include "Engine/LocalPlayer.h"

#include "UI/Match/GameResultWidget.h"
#include "UI/Title/TitleWidget.h"
#include "Engine/GameInstance.h"
#include "Lobby/LobbyRuntimeSubsystem.h"
#include "Settings/CursorSettingsLibrary.h"
#include "Definition/UI/WidgetClassDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TitleHUD)

void ATitleHUD::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PlayerController = GetOwningPlayerController();
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return;
	}

	if (!TitleWidget && TitleWidgetClass)
	{
		TitleWidget = CreateWidget<UTitleWidget>(PlayerController, TitleWidgetClass);
	}

	if (!TitleWidget)
	{

		return;
	}

	Screen = CreateWidget<UUiScreen>(PlayerController);
	FUIInputConfig Config(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
	Config.bIgnoreMoveInput = Config.bIgnoreLookInput = true;
	// 타이틀/방 목록의 종료는 기존 버튼이 담당한다.
	Screen->SetContent(TitleWidget, Config, EPdGameplayInputPolicy::Block, TitleWidget, FSimpleDelegate::CreateLambda([]() {}));
	PlayerController->GetLocalPlayer()->GetSubsystem<UUiSubsystem>()->PushScreen(Screen, EUiScreenLayer::Screen);
	UCursorSettingsLibrary::ApplyConfiguredMouseCursor(this, PlayerController);

	ShowPendingGameResult();
}

void ATitleHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Screen) Screen->DeactivateWidget();
	Screen = nullptr;
	if (GameResultWidget)
	{
		GameResultWidget->RemoveFromParent();
		GameResultWidget = nullptr;
	}

	if (TitleWidget)
	{
		TitleWidget->RemoveFromParent();
		TitleWidget = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void ATitleHUD::ShowPendingGameResult()
{
	APlayerController* PlayerController = GetOwningPlayerController();
	ULobbyRuntimeSubsystem* LobbySubsystem = UGameInstance::GetSubsystem<ULobbyRuntimeSubsystem>(GetGameInstance());
	if (!PlayerController || !PlayerController->IsLocalController() || !LobbySubsystem)
	{
		return;
	}

	FGameResultPresentationData GameResultData;
	if (!LobbySubsystem->ConsumePendingTitleGameResult(GameResultData))
	{
		return;
	}

	if (!GameResultWidgetClass)
	{
		if (const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this))
		{
			GameResultWidgetClass = WidgetDefinition->GetGameResultWidgetClass();
		}
	}

	if (!GameResultWidgetClass)
	{
		return;
	}

	GameResultWidget = CreateWidget<UGameResultWidget>(PlayerController, GameResultWidgetClass);
	if (!GameResultWidget)
	{
		return;
	}

	GameResultWidget->SetInfo(
		GameResultData.WinnerTitle,
		GameResultData.WinnerTeamColorIndex,
		GameResultData.MaxKillerName,
		GameResultData.MaxKillCount,
		GameResultData.PlayerStats);
	GameResultWidget->SetExitToLobbyEnabled(GameResultData.bAllowLobbyTravelOnExit);
	GameResultWidget->SetShowRewards(GameResultData.bShowRewards);
	GameResultWidget->ShowResultScreen();
}
