#include "UI/HudScoreboardLayer.h"
#include "UI/HudUiRouter.h"

#include "Engine/LocalPlayer.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Lobby/UI/GameResultWidget.h"
#include "Mode/PdHUD.h"
#include "Mode/PdPlayerController.h"
#include "Mode/PdPlayerState.h"
#include "TimerManager.h"
#include "UI/UiScreen.h"
#include "UI/UiSubsystem.h"
#include "Definition/UI/WidgetClassDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HudScoreboardLayer)

namespace
{
	constexpr float ScoreboardRefreshIntervalSeconds = 0.20f;

	FText ResolveScoreboardTeamName(const int32 TeamColorIndex)
	{
		switch (TeamColorIndex)
		{
		case 0: return NSLOCTEXT("GameResult", "TeamNameRed", "Red");
		case 1: return NSLOCTEXT("GameResult", "TeamNameBlue", "Blue");
		case 2: return NSLOCTEXT("GameResult", "TeamNameYellow", "Yellow");
		case 3: return NSLOCTEXT("GameResult", "TeamNamePurple", "Purple");
		case 4: return NSLOCTEXT("GameResult", "TeamNameGreen", "Green");
		case 5: return NSLOCTEXT("GameResult", "TeamNameOrange", "Orange");
		default: return NSLOCTEXT("GameResult", "TeamNameNone", "No Team");
		}
	}

	FText ResolveScoreboardPlayerName(const APlayerState* PlayerState)
	{
		if (!PlayerState)
		{
			return NSLOCTEXT("GameResult", "UnknownPlayerName", "Unknown");
		}

		if (const APdPlayerState* PdPlayerState = Cast<APdPlayerState>(PlayerState))
		{
			if (!PdPlayerState->GetPlayerMatchComponent()->GetMatchDisplayName().IsEmpty())
			{
				return PdPlayerState->GetPlayerMatchComponent()->GetMatchDisplayName();
			}
		}

		const FString PlayerName = PlayerState->GetPlayerName();
		return FText::FromString(PlayerName.IsEmpty() ? GetNameSafe(PlayerState) : PlayerName);
	}
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
        ScoreboardScreen = CreateWidget<UUiScreen>(Controller);
        FUIInputConfig Config(ECommonInputMode::All, EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown);
        Config.bIgnoreMoveInput = Config.bIgnoreLookInput = true;
        ScoreboardScreen->SetContent(ScoreboardWidget, Config, EPdGameplayInputPolicy::Block, ScoreboardWidget, FSimpleDelegate::CreateUObject(this, &ThisClass::Hide));
        Controller->GetLocalPlayer()->GetSubsystem<UUiSubsystem>()->PushScreen(ScoreboardScreen, EUiScreenLayer::Overlay);
    }

	if (UWorld* World = Hud->GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimerHandle);
		World->GetTimerManager().SetTimer(
			RefreshTimerHandle,
			this,
			&ThisClass::Refresh,
			ScoreboardRefreshIntervalSeconds,
			true);
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
	const AGameStateBase* CurrentGameState = World ? World->GetGameState() : nullptr;
	if (!CurrentGameState)
	{
		return;
	}

	for (APlayerState* PlayerState : CurrentGameState->PlayerArray)
	{
		const APdPlayerState* PdPlayerState = Cast<APdPlayerState>(PlayerState);
		if (!PdPlayerState)
		{
			continue;
		}

		FGameResultPlayerStat PlayerStat;
		PlayerStat.PlayerName = ResolveScoreboardPlayerName(PdPlayerState);
		PlayerStat.TeamColorIndex = PdPlayerState->GetPlayerMatchComponent()->GetMatchTeamColorIndex();
		PlayerStat.TeamName = ResolveScoreboardTeamName(PlayerStat.TeamColorIndex);
		PlayerStat.PlayerStateId = PdPlayerState->GetPlayerId();
		PlayerStat.KillCount = PdPlayerState->GetPlayerMatchComponent()->GetKillCount();
		PlayerStat.DeathCount = PdPlayerState->GetPlayerMatchComponent()->GetDeathCount();
		PlayerStat.GoldReward = 0;
		PlayerStat.bVictoryRewardEligible = false;
		OutPlayerStats.Add(PlayerStat);
	}

	OutPlayerStats.Sort([](const FGameResultPlayerStat& Left, const FGameResultPlayerStat& Right)
	{
		if (Left.KillCount != Right.KillCount)
		{
			return Left.KillCount > Right.KillCount;
		}
		if (Left.DeathCount != Right.DeathCount)
		{
			return Left.DeathCount < Right.DeathCount;
		}
		return Left.PlayerName.ToString() < Right.PlayerName.ToString();
	});
}
