#include "Component/Player/ControllerPresentationComponent.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Character/CharacterBase.h"
#include "Character/PdPlayer.h"
#include "Component/Experience/ExperiencePlayerProvisioningComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Lobby/Contents/LobbyHUD.h"
#include "Lobby/Contents/LobbyGameState.h"
#include "Lobby/Contents/LobbyPlayerController.h"
#include "Lobby/Contents/TitleHUD.h"
#include "Lobby/LobbyRuntimeSubsystem.h"
#include "Lobby/UI/LobbyWidget.h"
#include "Mode/ExperienceGameMode.h"
#include "Mode/PdGameInstance.h"
#include "Mode/PdHUD.h"
#include "Mode/PdPlayerController.h"
#include "Mode/PdPlayerState.h"
#include "Settings/LocalPlayerSettingsSubsystem.h"
#include "UI/KillLogTypes.h"
#include "UI/NotificationData.h"
#include "UI/UiSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ControllerPresentationComponent)

UControllerPresentationComponent::UControllerPresentationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UControllerPresentationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Shutdown();
	Super::EndPlay(EndPlayReason);
}

void UControllerPresentationComponent::InitializeLocalPresentation()
{
	APdPlayerController* Controller = GetPdController();
	if (!Controller || !Controller->IsLocalController())
	{
		return;
	}

	RestoreGameplayInputMode();
	if (ULocalPlayerSettingsSubsystem* LocalPlayerSettings = ULocalPlayerSettingsSubsystem::Get(Controller))
	{
		LocalPlayerSettings->ApplyLocalPlayerSettings(Controller);
	}
	if (UPdGameInstance* PdGameInstance = Controller->GetGameInstance<UPdGameInstance>())
	{
		PdGameInstance->RestoreWorldBgm();
	}
	if (Controller->IsA<ALobbyPlayerController>())
	{
		ULocalPlayer* LocalPlayer = Controller->GetLocalPlayer();
		if (UUiSubsystem* UiSubsystem =
			LocalPlayer ? LocalPlayer->GetSubsystem<UUiSubsystem>() : nullptr)
		{
			UiSubsystem->ShowLobbyEntryLoadingScreen();
		}
	}

	RefreshTravelLoadingScreen();
	ScheduleHideTravelLoadingScreenWhenReady();
	StartHealthBarVisibilityManagement();
}

void UControllerPresentationComponent::RefreshAfterPossession(
	APawn* PossessedPawn,
	const bool bRestoreCachedPaintFaceDecal)
{
	APdPlayerController* Controller = GetPdController();
	if (!Controller || !Controller->IsLocalController())
	{
		return;
	}

	ApplyCameraViewPitchClamp();
	RefreshTravelLoadingScreen();
	ScheduleHideTravelLoadingScreenWhenReady();
	ULocalPlayer* LocalPlayer = Controller->GetLocalPlayer();
	UUiSubsystem* UiSubsystem = LocalPlayer ? LocalPlayer->GetSubsystem<UUiSubsystem>() : nullptr;
	if ((!UiSubsystem
			|| (!UiSubsystem->IsTravelLoadingScreenActive()
				&& !UiSubsystem->HasActiveModalInput()))
		&& !Controller->IsA<ALobbyPlayerController>())
	{
		RestoreGameplayInputMode();
	}
	StartHealthBarVisibilityManagement();

	if (bRestoreCachedPaintFaceDecal)
	{
		if (APdPlayer* PlayerCharacter = Cast<APdPlayer>(PossessedPawn))
		{
			PlayerCharacter->RestoreCachedLobbyPaintCanvasFaceDecal();
		}
	}
}

void UControllerPresentationComponent::Shutdown()
{
	StopHealthBarVisibilityManagement();
	if (TravelLoadingReadyTickerHandle.IsValid())
	{
		FTSTicker::RemoveTicker(TravelLoadingReadyTickerHandle);
		TravelLoadingReadyTickerHandle.Reset();
	}
	SetTrainingRoomLoadingPaused(false);

	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		TimerManager.ClearTimer(RespawnTransformResetNextTickTimerHandle);
		TimerManager.ClearTimer(RespawnTransformResetRetryTimerHandle);
	}
	TravelLoadingHideRetryCount = 0;
}

void UControllerPresentationComponent::ApplyCameraViewPitchClamp() const
{
	if (APdPlayerController* Controller = GetPdController())
	{
		if (ULocalPlayerSettingsSubsystem* LocalPlayerSettings = ULocalPlayerSettingsSubsystem::Get(Controller))
		{
			LocalPlayerSettings->ApplyCameraViewPitchClamp(Controller);
		}
	}
}

void UControllerPresentationComponent::ShowRightNotification(
	const FPdNotificationData& NotificationData) const
{
	if (const APdPlayerController* Controller = GetPdControllerConst())
	{
		if (APdHUD* PdHUD = Controller->GetHUD<APdHUD>())
		{
			PdHUD->ShowRightNotification(NotificationData);
		}
	}
}

void UControllerPresentationComponent::AddKillLogEntry(const FKillLogEntry& KillLogEntry) const
{
	if (const APdPlayerController* Controller = GetPdControllerConst())
	{
		if (APdHUD* PdHUD = Controller->GetHUD<APdHUD>())
		{
			PdHUD->AddKillLogEntry(KillLogEntry);
		}
	}
}

void UControllerPresentationComponent::ShowGoldenKillAnnouncement(const FText& AnnouncementText) const
{
	if (const APdPlayerController* Controller = GetPdControllerConst())
	{
		if (APdHUD* PdHUD = Controller->GetHUD<APdHUD>())
		{
			PdHUD->ShowGoldenKillAnnouncement(AnnouncementText);
		}
	}
}

void UControllerPresentationComponent::StartRespawnDelayCountdown(const float DelaySeconds) const
{
	if (const APdPlayerController* Controller = GetPdControllerConst())
	{
		if (APdHUD* PdHUD = Controller->GetHUD<APdHUD>())
		{
			PdHUD->ShowRespawnDelay(FMath::Max(DelaySeconds, 0.0f));
		}
	}
}

void UControllerPresentationComponent::HideRespawnDelayCountdown() const
{
	if (const APdPlayerController* Controller = GetPdControllerConst())
	{
		if (APdHUD* PdHUD = Controller->GetHUD<APdHUD>())
		{
			PdHUD->HideRespawnDelay();
		}
	}
}

void UControllerPresentationComponent::ResetRespawnedPawnStateAtTransform(
	const FTransform& RespawnTransform)
{
	ResetRespawnedPawnStateForClientAtTransform(RespawnTransform);

	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		TimerManager.ClearTimer(RespawnTransformResetNextTickTimerHandle);
		TimerManager.ClearTimer(RespawnTransformResetRetryTimerHandle);

		FTimerDelegate NextTickDelegate;
		NextTickDelegate.BindUObject(
			this,
			&ThisClass::ResetRespawnedPawnStateForClientAtTransform,
			RespawnTransform);
		RespawnTransformResetNextTickTimerHandle =
			TimerManager.SetTimerForNextTick(NextTickDelegate);

		FTimerDelegate RetryDelegate;
		RetryDelegate.BindUObject(
			this,
			&ThisClass::ResetRespawnedPawnStateForClientAtTransform,
			RespawnTransform);
		TimerManager.SetTimer(
			RespawnTransformResetRetryTimerHandle,
			RetryDelegate,
			FMath::Max(Settings.RespawnStateResetRetryDelay, 0.01f),
			false);
	}
}

APdPlayerController* UControllerPresentationComponent::GetPdController() const
{
	return Cast<APdPlayerController>(GetOwner());
}

const APdPlayerController* UControllerPresentationComponent::GetPdControllerConst() const
{
	return Cast<APdPlayerController>(GetOwner());
}

void UControllerPresentationComponent::RestoreGameplayInputMode() const
{
	APdPlayerController* Controller = GetPdController();
	if (!Controller || !Controller->IsLocalController())
	{
		return;
	}

	UWidgetBlueprintLibrary::SetInputMode_GameOnly(Controller);
	Controller->bShowMouseCursor = false;
	Controller->bEnableClickEvents = false;
	Controller->bEnableMouseOverEvents = false;
}

void UControllerPresentationComponent::RefreshTravelLoadingScreen()
{
	APdPlayerController* Controller = GetPdController();
	if (!Controller || !Controller->IsLocalController())
	{
		return;
	}

	ULocalPlayer* LocalPlayer = Controller->GetLocalPlayer();
	UUiSubsystem* UiSubsystem = LocalPlayer ? LocalPlayer->GetSubsystem<UUiSubsystem>() : nullptr;
	if (UiSubsystem && UiSubsystem->IsTravelLoadingScreenActive())
	{
		UiSubsystem->ShowTravelLoadingScreen();
		SetTrainingRoomLoadingPaused(true);
	}
}

void UControllerPresentationComponent::ScheduleHideTravelLoadingScreenWhenReady()
{
	APdPlayerController* Controller = GetPdController();
	if (!Controller || !Controller->IsLocalController())
	{
		return;
	}

	ULocalPlayer* LocalPlayer = Controller->GetLocalPlayer();
	UUiSubsystem* UiSubsystem = LocalPlayer ? LocalPlayer->GetSubsystem<UUiSubsystem>() : nullptr;
	if (!UiSubsystem || !UiSubsystem->IsTravelLoadingScreenActive())
	{
		return;
	}

	TravelLoadingHideRetryCount = 0;
	SetTrainingRoomLoadingPaused(true);
	if (TravelLoadingReadyTickerHandle.IsValid())
	{
		FTSTicker::RemoveTicker(TravelLoadingReadyTickerHandle);
		TravelLoadingReadyTickerHandle.Reset();
	}
	TravelLoadingReadyTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(
			this,
			&ThisClass::TickTravelLoadingScreenReady),
		FMath::Max(Settings.TravelLoadingReadyCheckInterval, 0.01f));
}

bool UControllerPresentationComponent::TickTravelLoadingScreenReady(float)
{
	APdPlayerController* Controller = GetPdController();
	if (!Controller || !Controller->IsLocalController())
	{
		SetTrainingRoomLoadingPaused(false);
		TravelLoadingReadyTickerHandle.Reset();
		return false;
	}

	ULocalPlayer* LocalPlayer = Controller->GetLocalPlayer();
	UUiSubsystem* UiSubsystem = LocalPlayer ? LocalPlayer->GetSubsystem<UUiSubsystem>() : nullptr;
	if (!UiSubsystem || !UiSubsystem->IsTravelLoadingScreenActive())
	{
		SetTrainingRoomLoadingPaused(false);
		TravelLoadingReadyTickerHandle.Reset();
		return false;
	}
	SetTrainingRoomLoadingPaused(true);

	const bool bHasCharacterPawn = Cast<ACharacterBase>(Controller->GetPawn()) != nullptr;
	const bool bHasPlayerState = Controller->GetPlayerState<APdPlayerState>() != nullptr;
	const APdHUD* PdHUD = Controller->GetHUD<APdHUD>();
	const bool bHasPlayerHudWidget = PdHUD && PdHUD->GetPlayerHudWidget();
	const bool bIsTitleScreen = Controller->GetHUD<ATitleHUD>() != nullptr;
	const bool bBasePresentationReady =
		bIsTitleScreen
		|| (bHasCharacterPawn && bHasPlayerState && bHasPlayerHudWidget);
	const bool bStartupContentReady = UiSubsystem->IsStartupContentReady();

	bool bLobbyContentReady = true;
	bool bGameEntryContentReady = true;
	const bool bIsLobbyController = Controller->IsA<ALobbyPlayerController>();
	if (bIsLobbyController)
	{
		const UGameInstance* GameInstance = Controller->GetGameInstance();
		const ULobbyRuntimeSubsystem* LobbyRuntimeSubsystem =
			GameInstance
				? GameInstance->GetSubsystem<ULobbyRuntimeSubsystem>()
				: nullptr;
		const ALobbyGameState* LobbyGameState =
			GetWorld() ? GetWorld()->GetGameState<ALobbyGameState>() : nullptr;
		bLobbyContentReady =
			LobbyRuntimeSubsystem
			&& LobbyRuntimeSubsystem->IsLobbyEntryContentReady()
			&& LobbyGameState
			&& LobbyGameState->IsSelectedMapImageReady();
	}
	else
	{
		const UGameInstance* GameInstance = Controller->GetGameInstance();
		const ULobbyRuntimeSubsystem* LobbyRuntimeSubsystem =
			GameInstance
				? GameInstance->GetSubsystem<ULobbyRuntimeSubsystem>()
				: nullptr;
		if (LobbyRuntimeSubsystem
			&& LobbyRuntimeSubsystem->GetGameEntryContentPreloadResult()
				!= ELobbyContentPreloadResult::NotStarted)
		{
			bGameEntryContentReady =
				LobbyRuntimeSubsystem->IsGameEntryContentReady();
		}
	}

	const int32 MaxReadyCheckAttempts =
		FMath::Max(Settings.TravelLoadingReadyCheckMaxAttempts, 0);
	const bool bMayRetryBasePresentation =
		TravelLoadingHideRetryCount < MaxReadyCheckAttempts;
	if ((!bBasePresentationReady && (bIsLobbyController || bMayRetryBasePresentation))
		|| !bStartupContentReady
		|| !bLobbyContentReady
		|| !bGameEntryContentReady)
	{
		++TravelLoadingHideRetryCount;
		return true;
	}

	UiSubsystem->HideTravelLoadingScreen();
	SetTrainingRoomLoadingPaused(false);
	if (!bIsLobbyController)
	{
		if (UGameInstance* GameInstance = Controller->GetGameInstance())
		{
			if (ULobbyRuntimeSubsystem* LobbyRuntimeSubsystem =
				GameInstance->GetSubsystem<ULobbyRuntimeSubsystem>())
			{
				LobbyRuntimeSubsystem->ReleaseLobbyEntryContentPreload();
			}
		}
	}
	if (ALobbyPlayerController* LobbyController = Cast<ALobbyPlayerController>(Controller))
	{
		if (ALobbyHUD* LobbyHUD = LobbyController->GetHUD<ALobbyHUD>())
		{
			const ULobbyWidget* LobbyWidget = LobbyHUD->GetLobbyWidget();
			if (IsValid(LobbyWidget) && LobbyWidget->IsInViewport())
			{
				LobbyHUD->NotifyLobbyWidgetOpened();
			}
			else
			{
				LobbyHUD->NotifyLobbyWidgetClosed();
			}
		}
	}
	else
	{
		RestoreGameplayInputMode();
	}
	TravelLoadingHideRetryCount = 0;
	TravelLoadingReadyTickerHandle.Reset();
	return false;
}

void UControllerPresentationComponent::SetTrainingRoomLoadingPaused(
	const bool bPaused)
{
	APdPlayerController* Controller = GetPdController();
	UWorld* World = GetWorld();
	if (!Controller || !World)
	{
		if (!bPaused)
		{
			bAppliedTrainingRoomLoadingPause = false;
		}
		return;
	}

	if (bPaused)
	{
		const AExperienceGameMode* ExperienceGameMode =
			World->GetAuthGameMode<AExperienceGameMode>();
		const UExperiencePlayerProvisioningComponent* Provisioning =
			ExperienceGameMode
				? ExperienceGameMode->GetPlayerProvisioningComponent()
				: nullptr;
		if (bAppliedTrainingRoomLoadingPause
			|| !Provisioning
			|| !Provisioning->IsTrainingRoomMap()
			|| World->GetNetMode() != NM_Standalone
			|| UGameplayStatics::IsGamePaused(World))
		{
			return;
		}

		bAppliedTrainingRoomLoadingPause =
			UGameplayStatics::SetGamePaused(World, true);
		return;
	}

	if (bAppliedTrainingRoomLoadingPause)
	{
		UGameplayStatics::SetGamePaused(World, false);
		bAppliedTrainingRoomLoadingPause = false;
	}
}

void UControllerPresentationComponent::StartHealthBarVisibilityManagement()
{
	APdPlayerController* Controller = GetPdController();
	UWorld* World = GetWorld();
	if (!Controller || !Controller->IsLocalController() || !World)
	{
		return;
	}

	FTimerManager& TimerManager = World->GetTimerManager();
	TimerManager.ClearTimer(HealthBarVisibilityManagementTimerHandle);
	TimerManager.SetTimerForNextTick(this, &ThisClass::UpdateManagedHealthBarVisibility);
	TimerManager.SetTimer(
		HealthBarVisibilityManagementTimerHandle,
		this,
		&ThisClass::UpdateManagedHealthBarVisibility,
		FMath::Max(Settings.HealthBarVisibilityUpdateInterval, 0.01f),
		true);
}

void UControllerPresentationComponent::StopHealthBarVisibilityManagement()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HealthBarVisibilityManagementTimerHandle);
	}
}

void UControllerPresentationComponent::UpdateManagedHealthBarVisibility()
{
	APdPlayerController* Controller = GetPdController();
	UWorld* World = GetWorld();
	if (!Controller || !Controller->IsLocalController() || !World)
	{
		return;
	}

	FVector CameraLocation = FVector::ZeroVector;
	FRotator CameraRotation = FRotator::ZeroRotator;
	Controller->GetPlayerViewPoint(CameraLocation, CameraRotation);

	const float MaxDistance = FMath::Max(Settings.HealthBarVisibilityDistance, 0.0f);
	const float MaxDistanceSquared = MaxDistance > 0.0f ? FMath::Square(MaxDistance) : 0.0f;
	for (TActorIterator<ACharacterBase> It(World); It; ++It)
	{
		ACharacterBase* TargetCharacter = *It;
		if (!IsValid(TargetCharacter))
		{
			continue;
		}

		if (TargetCharacter == Controller->GetPawn())
		{
			TargetCharacter->SetHealthBarVisibleForLocalViewer(true);
			continue;
		}

		if (!ShouldManageHealthBarForTarget(TargetCharacter))
		{
			TargetCharacter->SetHealthBarVisibleForLocalViewer(false);
			continue;
		}

		TargetCharacter->UpdateHealthBarVisibilityForLocalViewer(
			Controller,
			CameraLocation,
			CameraRotation,
			MaxDistanceSquared);
	}
}

bool UControllerPresentationComponent::ShouldManageHealthBarForTarget(
	const ACharacterBase* TargetCharacter) const
{
	const APdPlayerController* Controller = GetPdControllerConst();
	if (!Controller || !TargetCharacter || TargetCharacter == Controller->GetPawn())
	{
		return false;
	}

	const ACharacterBase* LocalCharacter = Cast<ACharacterBase>(Controller->GetPawn());
	if (!LocalCharacter)
	{
		return false;
	}

	if (TargetCharacter->GetPlayerState())
	{
		return !LocalCharacter->IsSameTeam(TargetCharacter);
	}

	return true;
}

void UControllerPresentationComponent::ResetRespawnedPawnStateForClientAtTransform(
	FTransform RespawnTransform)
{
	APdPlayerController* Controller = GetPdController();
	ACharacterBase* RespawnedCharacter =
		Controller ? Cast<ACharacterBase>(Controller->GetPawn()) : nullptr;
	if (!Controller || !RespawnedCharacter)
	{
		return;
	}

	RespawnedCharacter->SetActorTransform(
		RespawnTransform,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	Controller->SetControlRotation(RespawnTransform.GetRotation().Rotator());
	ApplyCameraViewPitchClamp();
	RespawnedCharacter->ResetDeathStateForRespawn();
	if (Controller->IsLocalController())
	{
		Controller->SetViewTarget(RespawnedCharacter);
	}
}

void UControllerPresentationComponent::ShowInGameScoreboard()
{
	if (APdPlayerController* Controller = GetPdController())
	{
		if (Controller->IsLocalController())
		{
			if (APdHUD* PdHUD = Controller->GetHUD<APdHUD>())
			{
				PdHUD->ShowInGameScoreboard();
			}
		}
	}
}

void UControllerPresentationComponent::HideInGameScoreboard()
{
	if (APdPlayerController* Controller = GetPdController())
	{
		if (Controller->IsLocalController())
		{
			if (APdHUD* PdHUD = Controller->GetHUD<APdHUD>())
			{
				PdHUD->HideInGameScoreboard();
			}
		}
	}
}
