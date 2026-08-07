#include "Mode/ExperienceGameState.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Component/Experience/ExperienceManagerComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Lobby/UI/GameResultWidget.h"
#include "Definition/Match/MatchRuleDefinition.h"
#include "Mode/PdHUD.h"
#include "Mode/PdGameInstance.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Definition/UI/WidgetClassDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ExperienceGameState)

AExperienceGameState::AExperienceGameState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ExperienceManagerComponent = CreateDefaultSubobject<UExperienceManagerComponent>(TEXT("ExperienceManagerComponent"));
}

void AExperienceGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(AExperienceGameState, MatchRuleDefinition, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AExperienceGameState, MatchTimerState, Params);
}

void AExperienceGameState::SetMatchRuleDefinition(UMatchRuleDefinition* InMatchRuleDefinition)
{
	if (MatchRuleDefinition == InMatchRuleDefinition)
	{
		return;
	}

	MatchRuleDefinition = InMatchRuleDefinition;
	MARK_PROPERTY_DIRTY_FROM_NAME(AExperienceGameState, MatchRuleDefinition, this);
	ForceNetUpdate();
	RefreshLocalHudTimer();
}

void AExperienceGameState::OnRep_MatchRuleDefinition()
{
	RefreshLocalHudTimer();
}

void AExperienceGameState::SetMatchTimerState(
	const EMatchTimerPhase InPhase,
	const float InEndServerTimeSeconds)
{
	FReplicatedMatchTimerState NewState;
	NewState.Phase = InPhase;
	NewState.EndServerTimeSeconds = InPhase == EMatchTimerPhase::Running
		? FMath::Max(InEndServerTimeSeconds, 0.0f)
		: 0.0f;

	if (MatchTimerState == NewState)
	{
		return;
	}

	MatchTimerState = NewState;
	MARK_PROPERTY_DIRTY_FROM_NAME(AExperienceGameState, MatchTimerState, this);
	ForceNetUpdate();
	RefreshLocalHudTimer();
}

bool AExperienceGameState::TryGetMatchTimerRemainingSeconds(float& OutRemainingSeconds) const
{
	if (MatchTimerState.Phase == EMatchTimerPhase::Expired)
	{
		OutRemainingSeconds = 0.0f;
		return true;
	}

	if (MatchTimerState.Phase != EMatchTimerPhase::Running
		|| MatchTimerState.EndServerTimeSeconds <= 0.0f)
	{
		return false;
	}

	OutRemainingSeconds = FMath::Max(
		MatchTimerState.EndServerTimeSeconds - GetServerWorldTimeSeconds(),
		0.0f);
	return true;
}

void AExperienceGameState::OnRep_MatchTimerState()
{
	RefreshLocalHudTimer();
}

void AExperienceGameState::RefreshLocalHudTimer() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PlayerController = It->Get();
		if (!PlayerController || !PlayerController->IsLocalController())
		{
			continue;
		}

		if (APdHUD* PdHUD = Cast<APdHUD>(PlayerController->GetHUD()))
		{
			PdHUD->RefreshHudTimerVisibility();
		}
	}
}

void AExperienceGameState::Multicast_ShowGameResult_Implementation(
	const FText& WinnerTitle,
	const int32 WinnerTeamColorIndex,
	const FText& MaxKillerName,
	const int32 MaxKillCount,
	const TArray<FGameResultPlayerStat>& PlayerStats)
{
	UWorld* World = GetWorld();
	APlayerController* LocalPlayerController = World ? World->GetFirstPlayerController() : nullptr;
	if (!LocalPlayerController || !LocalPlayerController->IsLocalController())
	{
		return;
	}

	SaveLocalMatchRecord(LocalPlayerController, PlayerStats);

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

	UGameResultWidget* GameResultWidget = CreateWidget<UGameResultWidget>(LocalPlayerController, GameResultWidgetClass);
	if (!GameResultWidget)
	{

		return;
	}

	GameResultWidget->SetInfo(WinnerTitle, WinnerTeamColorIndex, MaxKillerName, MaxKillCount, PlayerStats);
	GameResultWidget->SetCloseOnlyOnExit(true);
	GameResultWidget->AddToViewport(100);

UWidgetBlueprintLibrary::SetInputMode_GameAndUIEx(LocalPlayerController, GameResultWidget, EMouseLockMode::DoNotLock, false, false);
	LocalPlayerController->bShowMouseCursor = true;
	LocalPlayerController->bEnableClickEvents = true;
	LocalPlayerController->bEnableMouseOverEvents = true;
}

void AExperienceGameState::SaveLocalMatchRecord(
	APlayerController* LocalPlayerController,
	const TArray<FGameResultPlayerStat>& PlayerStats) const
{
	if (!LocalPlayerController || !LocalPlayerController->IsLocalController())
	{
		return;
	}

	const APlayerState* LocalPlayerState = LocalPlayerController->PlayerState;
	const int32 LocalPlayerStateId = LocalPlayerState ? LocalPlayerState->GetPlayerId() : INDEX_NONE;
	const FGameResultPlayerStat* LocalPlayerStat = PlayerStats.FindByPredicate(
		[LocalPlayerStateId](const FGameResultPlayerStat& PlayerStat)
		{
			return PlayerStat.PlayerStateId != INDEX_NONE && PlayerStat.PlayerStateId == LocalPlayerStateId;
		});

	if (!LocalPlayerStat)
	{

		return;
	}

	UPdGameInstance* PdGameInstance = GetGameInstance<UPdGameInstance>();
	if (!PdGameInstance)
	{

		return;
	}

	FString PlayerId = PdGameInstance->GetPreferredSavePlayerId();
	PlayerId.TrimStartAndEndInline();
	if (PlayerId.IsEmpty())
	{
		PlayerId = PdGameInstance->ResolveSavePlayerId(
			LocalPlayerController,
			LocalPlayerState);
	}
	if (PlayerId.IsEmpty())
	{
		PlayerId = PdGameInstance->GetLocalClientSavePlayerId();
	}
	PlayerId.TrimStartAndEndInline();
	if (PlayerId.IsEmpty())
	{

		return;
	}

	FMatchRecord MatchRecord;
	MatchRecord.bWin = LocalPlayerStat->bVictoryRewardEligible;
	MatchRecord.KillCount = LocalPlayerStat->KillCount;
	MatchRecord.DeathCount = LocalPlayerStat->DeathCount;
	MatchRecord.Reward = LocalPlayerStat->GoldReward;

	PdGameInstance->SetPreferredSavePlayerId(PlayerId);
	PdGameInstance->AddMatchRecord(PlayerId, MatchRecord, true);

}
