#include "Component/Player/ControllerPresentationComponent.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Character/CharacterBase.h"
#include "Character/PdPlayer.h"
#include "Component/Experience/ExperiencePlayerProvisioningComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Lobby/Contents/LobbyHUD.h"
#include "Lobby/Contents/LobbyGameState.h"
#include "Lobby/Contents/TitleHUD.h"
#include "Lobby/LobbyRuntimeSubsystem.h"
#include "Lobby/UI/LobbyWidget.h"
#include "Mode/ExperienceGameMode.h"
#include "Engine/GameInstance.h"
#include "Settings/BgmSubsystem.h"
#include "Mode/PdHUD.h"
#include "Mode/PdPlayerController.h"
#include "Mode/PdPlayerState.h"
#include "Settings/LocalPlayerSettingsSubsystem.h"
#include "ShaderPipelineCache.h"
#include "UI/KillLogTypes.h"
#include "UI/NotificationData.h"
#include "UI/UiSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ControllerPresentationComponent)

// 로컬 화면 처리에 필요한 상태만 보관하고 상시 Tick과 네트워크 복제는 사용하지 않는다.
UControllerPresentationComponent::UControllerPresentationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

// 컨트롤러가 종료되면 화면 갱신 타이머와 로딩 중 일시정지를 해제한다.
void UControllerPresentationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Shutdown();
	Super::EndPlay(EndPlayReason);
}

// 늦게 로드된 설정을 실행 중인 체력바와 로딩 화면의 갱신 주기에도 적용한다.
void UControllerPresentationComponent::ApplySettings(const FControllerPresentationSettings& InSettings)
{
	const bool bHealthBarIntervalChanged = Settings.HealthBarVisibilityUpdateInterval != InSettings.HealthBarVisibilityUpdateInterval;
	const bool bTravelIntervalChanged = Settings.TravelLoadingReadyCheckInterval != InSettings.TravelLoadingReadyCheckInterval;
	Settings = InSettings;

	if (UWorld* World = GetWorld(); bHealthBarIntervalChanged && World
		&& World->GetTimerManager().TimerExists(HealthBarVisibilityManagementTimerHandle))
	{
		StartHealthBarVisibilityManagement();
	}
	if (bTravelIntervalChanged && TravelLoadingReadyTickerHandle.IsValid())
	{
		// 설정 변경은 로딩 대기 횟수를 초기화하지 않고 실행 주기만 바꾼다.
		UpdateTravelLoadingReadyTicker();
	}
}

// 로컬 입력 모드·카메라 설정·배경음악을 적용하고 로딩 화면과 체력바 갱신을 시작한다.
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
	if (UBgmSubsystem* BgmSubsystem = UGameInstance::GetSubsystem<UBgmSubsystem>(Controller->GetGameInstance()))
	{
		BgmSubsystem->RestoreWorldBgm();
	}
	if (Controller->UsesLobbyPresentation())
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

// 새로 조종하는 Pawn에 맞춰 카메라와 로딩 화면을 갱신하고 필요한 얼굴 데칼을 복원한다.
void UControllerPresentationComponent::RefreshAfterPossession(APawn* PossessedPawn)
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
		&& !Controller->UsesLobbyPresentation())
	{
		RestoreGameplayInputMode();
	}
	StartHealthBarVisibilityManagement();

	if (!Controller->UsesLobbyPresentation())
	{
		if (APdPlayer* PlayerCharacter = Cast<APdPlayer>(PossessedPawn))
		{
			PlayerCharacter->RestoreCachedLobbyPaintCanvasFaceDecal();
		}
	}
}

// 화면 처리에 등록한 타이머와 티커를 해제하고 이 컴포넌트가 건 일시정지만 되돌린다.
void UControllerPresentationComponent::Shutdown()
{
	StopHealthBarVisibilityManagement();
	if (TravelLoadingReadyTickerHandle.IsValid())
	{
		FTSTicker::RemoveTicker(TravelLoadingReadyTickerHandle);
		TravelLoadingReadyTickerHandle.Reset();
	}
	SetTrainingRoomLoadingPaused(false);

	TravelLoadingHideRetryCount = 0;
}

// 플레이어 설정에 맞춰 카메라의 상하 회전 범위를 적용한다.
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

// 로컬 HUD에 보상이나 상태 알림을 전달한다.
void UControllerPresentationComponent::ShowRightNotification(
	const FPdNotificationData& NotificationData) const
{
	if (const APdPlayerController* Controller = GetPdController())
	{
		if (APdHUD* PdHUD = Controller->GetHUD<APdHUD>())
		{
			PdHUD->ShowRightNotification(NotificationData);
		}
	}
}

// 로컬 HUD의 킬 로그에 처치 기록을 추가한다.
void UControllerPresentationComponent::AddKillLogEntry(const FKillLogEntry& KillLogEntry) const
{
	if (const APdPlayerController* Controller = GetPdController())
	{
		if (APdHUD* PdHUD = Controller->GetHUD<APdHUD>())
		{
			PdHUD->AddKillLogEntry(KillLogEntry);
		}
	}
}

// 골든 킬 안내 문구를 로컬 HUD에 표시한다.
void UControllerPresentationComponent::ShowGoldenKillAnnouncement(const FText& AnnouncementText) const
{
	if (const APdPlayerController* Controller = GetPdController())
	{
		if (APdHUD* PdHUD = Controller->GetHUD<APdHUD>())
		{
			PdHUD->ShowGoldenKillAnnouncement(AnnouncementText);
		}
	}
}

// 부활까지 남은 대기 시간을 HUD에 표시한다.
void UControllerPresentationComponent::StartRespawnDelayCountdown(const float DelaySeconds) const
{
	if (const APdPlayerController* Controller = GetPdController())
	{
		if (APdHUD* PdHUD = Controller->GetHUD<APdHUD>())
		{
			PdHUD->ShowRespawnDelay(FMath::Max(DelaySeconds, 0.0f));
		}
	}
}

// 부활 대기 표시를 숨긴다.
void UControllerPresentationComponent::HideRespawnDelayCountdown() const
{
	if (const APdPlayerController* Controller = GetPdController())
	{
		if (APdHUD* PdHUD = Controller->GetHUD<APdHUD>())
		{
			PdHUD->HideRespawnDelay();
		}
	}
}

// 화면 처리를 소유한 프로젝트 컨트롤러를 조회한다.
APdPlayerController* UControllerPresentationComponent::GetPdController() const
{
	return Cast<APdPlayerController>(GetOwner());
}

// 모달 화면이 없는 플레이 상태로 입력 모드와 마우스 커서를 복원한다.
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

// 맵 이동 로딩 화면을 유지하고 필요한 경우 훈련실 진행을 일시정지한다.
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

// 화면과 콘텐츠가 준비되면 로딩 화면을 닫도록 준비 상태 확인을 시작한다.
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
	UpdateTravelLoadingReadyTicker();
}

// 기존 준비 확인 티커를 현재 설정 주기로 교체한다.
void UControllerPresentationComponent::UpdateTravelLoadingReadyTicker()
{
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

// 화면·로비·진입 콘텐츠 준비를 확인한 뒤 로딩 화면을 닫고 입력과 게임 진행을 복원한다.
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
	const bool bIsLobbyController = Controller->UsesLobbyPresentation();
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

	// 콘텐츠 로드가 끝나도 등록된 PSO의 비동기 컴파일이 남아 있으면 화면을 유지한다.
	if (FShaderPipelineCache::NumPrecompilesRemaining() > 0)
	{
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
	if (bIsLobbyController)
	{
		if (ALobbyHUD* LobbyHUD = Controller->GetHUD<ALobbyHUD>())
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

// 혼자 실행하는 훈련실에서만 로딩 중 게임 진행을 멈추고 이후 재개한다.
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

// 로컬 시점에서 보이는 캐릭터 체력바를 즉시 갱신하고 주기 갱신을 예약한다.
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
	UpdateManagedHealthBarVisibility();
	TimerManager.SetTimer(
		HealthBarVisibilityManagementTimerHandle,
		this,
		&ThisClass::UpdateManagedHealthBarVisibility,
		FMath::Max(Settings.HealthBarVisibilityUpdateInterval, 0.01f),
		true);
}

// 컨트롤러 종료 시 체력바 주기 갱신을 중단한다.
void UControllerPresentationComponent::StopHealthBarVisibilityManagement()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HealthBarVisibilityManagementTimerHandle);
	}
}

// 관찰자의 위치·시선·팀 관계에 맞춰 각 캐릭터의 체력바 가시성을 갱신한다.
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

// 아군을 제외하고 체력바를 표시할 대상 캐릭터인지 판단한다.
bool UControllerPresentationComponent::ShouldManageHealthBarForTarget(
	const ACharacterBase* TargetCharacter) const
{
	const APdPlayerController* Controller = GetPdController();
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

// 서버가 지정한 Pawn을 현재 조종 중일 때만 부활 시선과 카메라 대상을 복구한다.
void UControllerPresentationComponent::RefreshAfterRespawn(APawn* RespawnedPawn, const FRotator& RespawnRotation)
{
	APdPlayerController* Controller = GetPdController();
	if (!Controller || !Controller->IsLocalController() || !IsValid(RespawnedPawn) || Controller->GetPawn() != RespawnedPawn)
	{
		// 아직 빙의가 반영되지 않은 새 Pawn은 엔진의 빙의 완료 경로에서 카메라를 준비한다.
		return;
	}

	Controller->SetControlRotation(RespawnRotation);
	ApplyCameraViewPitchClamp();
	Controller->SetViewTarget(RespawnedPawn);
}

// 소유 플레이어의 점수판을 연다.
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

// 소유 플레이어의 점수판을 닫는다.
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
