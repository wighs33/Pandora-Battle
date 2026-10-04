#include "UI/HUD/HudScoreboardLayer.h"
#include "UI/HUD/HudUiRouter.h"

#include "Engine/LocalPlayer.h"
#include "GameFramework/GameStateBase.h"
#include "Component/Match/MatchResultReport.h"
#include "UI/Match/GameResultWidget.h"
#include "UI/HUD/PdHUD.h"
#include "Mode/PdPlayerController.h"
#include "TimerManager.h"
#include "UI/Core/UiScreen.h"
#include "UI/Core/UiSubsystem.h"
#include "UI/Core/WidgetClassDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HudScoreboardLayer)

namespace
{
	constexpr float ScoreboardRefreshIntervalSeconds = 0.20f;
}

void UHudScoreboardLayer::Initialize(APdHUD* InOwnerHud, UHudUiRouter* InRouter)
{
	OwnerHud = InOwnerHud;
	Router = InRouter;
}

void UHudScoreboardLayer::Show()
{
	APdHUD* Hud = OwnerHud.Get();
	const UHudUiRouter* UiRouter = Router.Get();
	APdPlayerController* Controller = Hud ? Hud->GetPdController() : nullptr;
	const UWidgetClassDefinition* Definition = UiRouter ? UiRouter->GetActiveDefinition() : nullptr;
	if (!Hud || !Controller || !Controller->IsLocalController() || !Definition
		|| Hud->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (!ScoreboardWidget)
	{
		if (const TSubclassOf<UGameResultWidget> WidgetClass = Definition->GetGameResultWidgetClass())
		{
			ScoreboardWidget = CreateWidget<UGameResultWidget>(Controller, WidgetClass);
		}
	}
	if (!ScoreboardWidget)
	{
		return;
	}

	Refresh();
	if (!ScoreboardScreen)
    {
        ScoreboardScreen = UUiScreen::CreateBlocking(Controller, ScoreboardWidget, ScoreboardWidget,
            FSimpleDelegate::CreateUObject(this, &ThisClass::Hide), ECommonInputMode::All,
            EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown);
        Controller->GetLocalPlayer()->GetSubsystem<UUiSubsystem>()->PushScreen(ScoreboardScreen, EUiScreenLayer::Overlay);
    }

	if (UWorld* World = Hud->GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimerHandle);
		World->GetTimerManager().SetTimer(RefreshTimerHandle, this, &ThisClass::Refresh,
			ScoreboardRefreshIntervalSeconds, true);
	}

	Hud->RefreshPlayerHudVisibility();
}

void UHudScoreboardLayer::Hide()
{
    if (ScoreboardScreen)
    {
        ScoreboardScreen->DeactivateWidget();
        ScoreboardScreen = nullptr;
    }

	if (APdHUD* Hud = OwnerHud.Get())
	{
		Hud->GetWorldTimerManager().ClearTimer(RefreshTimerHandle);
	}
	RefreshTimerHandle.Invalidate();

	if (ScoreboardWidget)
	{
		ScoreboardWidget->RemoveFromParent();
	}

	if (APdHUD* Hud = OwnerHud.Get())
	{
		Hud->RefreshPlayerHudVisibility();
	}
}

bool UHudScoreboardLayer::IsOpen() const
{
	return ScoreboardScreen && ScoreboardScreen->IsActivated();
}

void UHudScoreboardLayer::Refresh()
{
	if (!ScoreboardWidget)
	{
		return;
	}

	TArray<FGameResultPlayerStat> PlayerStats;
	BuildPlayerStats(PlayerStats);
	ScoreboardWidget->SetInGameScoreboardInfo(PlayerStats);
}

void UHudScoreboardLayer::Shutdown()
{
	Hide();
	ScoreboardWidget = nullptr;
}

void UHudScoreboardLayer::BuildPlayerStats(TArray<FGameResultPlayerStat>& OutPlayerStats) const
{
	OutPlayerStats.Reset();

	const APdHUD* Hud = OwnerHud.Get();
	const UWorld* World = Hud ? Hud->GetWorld() : nullptr;
	if (const AGameStateBase* CurrentGameState = World ? World->GetGameState() : nullptr)
	{
		OutPlayerStats = MatchResultReport::BuildPlayerStats(*CurrentGameState);
	}
}
