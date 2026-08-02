#include "Lobby/Contents/TitleHUD.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Lobby/UI/GameResultWidget.h"
#include "Lobby/UI/TitleWidget.h"
#include "Mode/PdGameInstance.h"
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

	TitleWidget->AddToViewport();
	UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(PlayerController, nullptr, EMouseLockMode::DoNotLock, false);
	PlayerController->bShowMouseCursor = true;
	PlayerController->bEnableClickEvents = true;
	PlayerController->bEnableMouseOverEvents = true;
	UCursorSettingsLibrary::ApplyConfiguredMouseCursor(this, PlayerController);

	ShowPendingGameResult();
}

void ATitleHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
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
	UPdGameInstance* PdGameInstance = GetGameInstance<UPdGameInstance>();
	if (!PlayerController || !PlayerController->IsLocalController() || !PdGameInstance)
	{
		return;
	}

	FGameResultPresentationData GameResultData;
	if (!PdGameInstance->ConsumePendingTitleGameResult(GameResultData))
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
	GameResultWidget->AddToViewport(100);

	UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(PlayerController, GameResultWidget, EMouseLockMode::DoNotLock, false);
	PlayerController->bShowMouseCursor = true;
	PlayerController->bEnableClickEvents = true;
	PlayerController->bEnableMouseOverEvents = true;
}
