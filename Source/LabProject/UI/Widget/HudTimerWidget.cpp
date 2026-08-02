#include "UI/Widget/HudTimerWidget.h"

#include "Components/TextBlock.h"
#include "Data/ContentDataSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Mode/ExperienceGameState.h"
#include "Definition/Match/MatchRuleDefinition.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HudTimerWidget)

UHudTimerWidget::UHudTimerWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UHudTimerWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!BeginMatchRulePreload())
	{
		StartTimer();
	}
}

void UHudTimerWidget::NativeDestruct()
{
	ReleaseMatchRulePreload();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimerTickHandle);
	}

	Super::NativeDestruct();
}

bool UHudTimerWidget::BeginMatchRulePreload()
{
	ReleaseMatchRulePreload();

	const UWorld* World = GetWorld();
	const AExperienceGameState* ExperienceGameState =
		World ? World->GetGameState<AExperienceGameState>() : nullptr;
	if ((ExperienceGameState && ExperienceGameState->GetMatchRuleDefinition())
		|| MatchRuleDefinition.IsNull()
		|| MatchRuleDefinition.Get())
	{
		return false;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	UContentDataSubsystem* ContentSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
	if (!ContentSubsystem)
	{
		return false;
	}

	const int32 PreloadGeneration = ++MatchRulePreloadGeneration;
	MatchRulePreloadHandle =
		ContentSubsystem->PreloadSoftObjectPathsAsync(
			{MatchRuleDefinition.ToSoftObjectPath()},
			FSimpleDelegate::CreateWeakLambda(
				this,
				[this, PreloadGeneration]()
				{
					if (PreloadGeneration == MatchRulePreloadGeneration)
					{
						StartTimer();
					}
				}));
	return true;
}

void UHudTimerWidget::ReleaseMatchRulePreload()
{
	++MatchRulePreloadGeneration;
	if (MatchRulePreloadHandle.IsValid())
	{
		MatchRulePreloadHandle->CancelHandle();
		MatchRulePreloadHandle->ReleaseHandle();
		MatchRulePreloadHandle.Reset();
	}
}

void UHudTimerWidget::StartTimer()
{
	if (ShouldSuppressTimer())
	{
		PauseTimer();
		ResetTimer();
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	if (bRefreshTimerActive)
	{
		SyncFromReplicatedTimerState();
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const UMatchRuleDefinition* MatchRules = GetMatchRuleDefinition();
	bRefreshTimerActive = true;
	SetVisibility(ESlateVisibility::HitTestInvisible);
	World->GetTimerManager().SetTimer(
		TimerTickHandle,
		this,
		&ThisClass::HandleTimerTick,
		FMath::Max(MatchRules ? MatchRules->HudTickInterval : 0.1f, 0.01f),
		true);

	SyncFromReplicatedTimerState();
}

void UHudTimerWidget::PauseTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimerTickHandle);
	}

	bRefreshTimerActive = false;
	RefreshUI();
}

void UHudTimerWidget::StopTimer(const bool bResetTimer)
{
	PauseTimer();

	if (bResetTimer)
	{
		ResetTimer();
	}
}

void UHudTimerWidget::ResetTimer()
{
	const float ConfiguredTimerSeconds = FMath::Max(GetConfiguredTimerSeconds(), 0.0f);
	CurrentTimerSeconds = IsCountDownTimer()
		? ConfiguredTimerSeconds
		: 0.0f;
	SyncFromReplicatedTimerState();
	RefreshUI();
}

void UHudTimerWidget::ForceMoveOwningPawnNow()
{
	// Intentionally empty. Gameplay transitions are executed only by the authoritative GameMode.
}

bool UHudTimerWidget::IsTimerRunning() const
{
	const UWorld* World = GetWorld();
	const AExperienceGameState* ExperienceGameState =
		World ? World->GetGameState<AExperienceGameState>() : nullptr;
	return ExperienceGameState
		&& ExperienceGameState->GetMatchTimerPhase() == EPdMatchTimerPhase::Running;
}

void UHudTimerWidget::RefreshUI()
{
	if (!Txt_Timer)
	{
		return;
	}

	Txt_Timer->SetText(FormatTimerText());

	const bool bUseWarningColor = bUseWarningTextColor
		&& IsCountDownTimer()
		&& CurrentTimerSeconds > 0.0f
		&& CurrentTimerSeconds <= WarningThresholdSeconds;
	Txt_Timer->SetColorAndOpacity(FSlateColor(bUseWarningColor ? WarningTextColor : NormalTextColor));
}

void UHudTimerWidget::HandleTimerTick()
{
	if (ShouldSuppressTimer())
	{
		PauseTimer();
		ResetTimer();
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	if (!bRefreshTimerActive)
	{
		return;
	}

	SyncFromReplicatedTimerState();
	RefreshUI();
}

void UHudTimerWidget::SyncFromReplicatedTimerState()
{
	const UWorld* World = GetWorld();
	const AExperienceGameState* ExperienceGameState =
		World ? World->GetGameState<AExperienceGameState>() : nullptr;
	if (!ExperienceGameState)
	{
		return;
	}

	const float ConfiguredTimerSeconds = FMath::Max(GetConfiguredTimerSeconds(), 0.0f);
	const bool bCountDownTimer = IsCountDownTimer();
	float AuthoritativeRemainingSeconds = 0.0f;
	switch (ExperienceGameState->GetMatchTimerPhase())
	{
	case EPdMatchTimerPhase::Running:
		if (ExperienceGameState->TryGetMatchTimerRemainingSeconds(AuthoritativeRemainingSeconds))
		{
			CurrentTimerSeconds = bCountDownTimer
				? AuthoritativeRemainingSeconds
				: FMath::Clamp(
					ConfiguredTimerSeconds - AuthoritativeRemainingSeconds,
					0.0f,
					ConfiguredTimerSeconds);
		}
		break;

	case EPdMatchTimerPhase::Expired:
		CurrentTimerSeconds = bCountDownTimer ? 0.0f : ConfiguredTimerSeconds;
		if (const UMatchRuleDefinition* MatchRules = GetMatchRuleDefinition();
			MatchRules && MatchRules->bHudHideWhenFinished)
		{
			SetVisibility(ESlateVisibility::Collapsed);
		}
		break;

	case EPdMatchTimerPhase::Inactive:
	case EPdMatchTimerPhase::Suppressed:
	default:
		CurrentTimerSeconds = bCountDownTimer ? ConfiguredTimerSeconds : 0.0f;
		break;
	}
}

FText UHudTimerWidget::FormatTimerText() const
{
	const bool bCountDownTimer = IsCountDownTimer();
	if (bCountDownTimer && CurrentTimerSeconds <= 0.0f)
	{
		return FinishedText;
	}

	const int32 TotalSeconds = bCountDownTimer
		? FMath::CeilToInt(FMath::Max(CurrentTimerSeconds, 0.0f))
		: FMath::FloorToInt(FMath::Max(CurrentTimerSeconds, 0.0f));
	const int32 Hours = TotalSeconds / 3600;
	const int32 Minutes = (TotalSeconds % 3600) / 60;
	const int32 Seconds = TotalSeconds % 60;

	if (bShowHoursWhenNeeded && Hours > 0)
	{
		return FText::FromString(FString::Printf(TEXT("%02d:%02d:%02d"), Hours, Minutes, Seconds));
	}

	return FText::FromString(FString::Printf(TEXT("%02d:%02d"), Minutes + (Hours * 60), Seconds));
}

const UMatchRuleDefinition* UHudTimerWidget::GetMatchRuleDefinition() const
{
	const UWorld* World = GetWorld();
	const AExperienceGameState* ExperienceGameState =
		World ? World->GetGameState<AExperienceGameState>() : nullptr;
	if (ExperienceGameState && ExperienceGameState->GetMatchRuleDefinition())
	{
		return ExperienceGameState->GetMatchRuleDefinition();
	}

	if (!MatchRuleDefinition.IsNull())
	{
		if (const UMatchRuleDefinition* LoadedMatchRules = MatchRuleDefinition.Get())
		{
			return LoadedMatchRules;
		}
	}

	return GetDefault<UMatchRuleDefinition>();
}

bool UHudTimerWidget::ShouldSuppressTimer() const
{
	const UWorld* World = GetWorld();
	const AExperienceGameState* ExperienceGameState =
		World ? World->GetGameState<AExperienceGameState>() : nullptr;
	if (ExperienceGameState)
	{
		const EPdMatchTimerPhase TimerPhase = ExperienceGameState->GetMatchTimerPhase();
		return TimerPhase == EPdMatchTimerPhase::Inactive
			|| TimerPhase == EPdMatchTimerPhase::Suppressed;
	}

	return ShouldSuppressTimerForCurrentMap();
}

bool UHudTimerWidget::ShouldSuppressTimerForCurrentMap() const
{
	const UMatchRuleDefinition* MatchRules = GetMatchRuleDefinition();
	if (!MatchRules || MatchRules->MapsWithoutMatchTimer.IsEmpty())
	{
		return false;
	}

	const FString CurrentLevelName = UGameplayStatics::GetCurrentLevelName(this, true);
	return MatchRules->MapsWithoutMatchTimer.Contains(FName(*CurrentLevelName));
}

float UHudTimerWidget::GetConfiguredTimerSeconds() const
{
	const UMatchRuleDefinition* MatchRules = GetMatchRuleDefinition();
	return MatchRules ? MatchRules->MatchTimerSeconds : 0.0f;
}

bool UHudTimerWidget::IsCountDownTimer() const
{
	const UMatchRuleDefinition* MatchRules = GetMatchRuleDefinition();
	return !MatchRules || MatchRules->bHudCountDown;
}
