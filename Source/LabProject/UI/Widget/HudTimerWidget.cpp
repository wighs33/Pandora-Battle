#include "UI/Widget/HudTimerWidget.h"

#include "Components/TextBlock.h"
#include "Data/ContentDataSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Mode/ExperienceGameState.h"
#include "Definition/Level/LevelDefinition.h"
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
	CurrentTimerSeconds = ConfiguredTimerSeconds;
	SyncFromReplicatedTimerState();
	RefreshUI();
}

bool UHudTimerWidget::IsTimerRunning() const
{
	const UWorld* World = GetWorld();
	const AExperienceGameState* ExperienceGameState =
		World ? World->GetGameState<AExperienceGameState>() : nullptr;
	return ExperienceGameState
		&& ExperienceGameState->GetMatchTimerPhase() == EMatchTimerPhase::Running;
}

void UHudTimerWidget::RefreshUI()
{
	if (!Txt_Timer)
	{
		return;
	}

	Txt_Timer->SetText(FormatTimerText());

	const bool bUseWarningColor = bUseWarningTextColor
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
	float AuthoritativeRemainingSeconds = 0.0f;
	switch (ExperienceGameState->GetMatchTimerPhase())
	{
	case EMatchTimerPhase::Running:
		if (ExperienceGameState->TryGetMatchTimerRemainingSeconds(AuthoritativeRemainingSeconds))
		{
			CurrentTimerSeconds = AuthoritativeRemainingSeconds;
		}
		break;

	case EMatchTimerPhase::Expired:
		CurrentTimerSeconds = 0.0f;
		if (const UMatchRuleDefinition* MatchRules = GetMatchRuleDefinition();
			MatchRules && MatchRules->bHudHideWhenFinished)
		{
			SetVisibility(ESlateVisibility::Collapsed);
		}
		break;

	case EMatchTimerPhase::Inactive:
	case EMatchTimerPhase::Suppressed:
	default:
		CurrentTimerSeconds = ConfiguredTimerSeconds;
		break;
	}
}

FText UHudTimerWidget::FormatTimerText() const
{
	if (CurrentTimerSeconds <= 0.0f)
	{
		return FinishedText;
	}

	const int32 TotalSeconds = FMath::CeilToInt(FMath::Max(CurrentTimerSeconds, 0.0f));
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

	return UMatchRuleDefinition::ResolveDefaultDefinition();
}

bool UHudTimerWidget::ShouldSuppressTimer() const
{
	const UWorld* World = GetWorld();
	const AExperienceGameState* ExperienceGameState =
		World ? World->GetGameState<AExperienceGameState>() : nullptr;
	if (ExperienceGameState)
	{
		const EMatchTimerPhase TimerPhase = ExperienceGameState->GetMatchTimerPhase();
		return TimerPhase == EMatchTimerPhase::Inactive
			|| TimerPhase == EMatchTimerPhase::Suppressed;
	}

	return ShouldSuppressTimerForCurrentMap();
}

bool UHudTimerWidget::ShouldSuppressTimerForCurrentMap() const
{
	const UMatchRuleDefinition* MatchRules = GetMatchRuleDefinition();
	const FString CurrentLevelName = UGameplayStatics::GetCurrentLevelName(this, true);
	const ULevelDefinition* Levels = ULevelDefinition::ResolveDefaultDefinition();
	return (Levels && Levels->IsTrainingRoomMapName(CurrentLevelName))
		|| (MatchRules
			&& MatchRules->MapsWithoutMatchTimer.Contains(FName(*CurrentLevelName)));
}

float UHudTimerWidget::GetConfiguredTimerSeconds() const
{
	const UMatchRuleDefinition* MatchRules = GetMatchRuleDefinition();
	return MatchRules ? MatchRules->MatchTimerSeconds : 0.0f;
}
