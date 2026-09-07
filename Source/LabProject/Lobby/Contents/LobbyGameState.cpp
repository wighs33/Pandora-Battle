#include "Lobby/Contents/LobbyGameState.h"

#include "Component/Experience/ExperienceManagerComponent.h"
#include "Engine/GameInstance.h"
#include "Component/Lobby/LobbyPlayerStateComponent.h"
#include "Lobby/Contents/LobbyPlayerState.h"
#include "Lobby/LobbyRuntimeSubsystem.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LobbyGameState)

ALobbyGameState::ALobbyGameState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ExperienceManagerComponent = CreateDefaultSubobject<UExperienceManagerComponent>(TEXT("ExperienceManagerComponent"));
	SetNetUpdateFrequency(30.0f);
}

// 로컬 Experience 준비 결과도 관찰해 각 클라이언트의 실패 안내를 갱신한다.
void ALobbyGameState::BeginPlay()
{
	Super::BeginPlay();
	ExperienceManagerComponent->CallOrRegister_OnExperienceLoaded(
		FOnPdExperienceLoaded::FDelegate::CreateUObject(this, &ThisClass::HandleExperienceLoaded));
	ExperienceManagerComponent->CallOrRegister_OnExperienceLoadFailed(
		FOnPdExperienceLoadFailed::FDelegate::CreateUObject(this, &ThisClass::HandleExperienceLoadFailed));
}

// 월드 종료 후 참가자 변경 알림이 이전 로비로 전달되지 않도록 구독을 해제한다.
void ALobbyGameState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (APlayerState* PlayerState : PlayerArray)
	{
		if (ALobbyPlayerState* LobbyPlayerState = Cast<ALobbyPlayerState>(PlayerState))
		{
			LobbyPlayerState->GetLobbyPlayerStateComponent()->OnLobbyRuntimeStateChanged.RemoveAll(this);
		}
	}
	OnLobbyStateChanged.Clear();
	Super::EndPlay(EndPlayReason);
}

// 참가자가 실제 로컬 목록에 등록되는 시점부터 이름·팀·퇴장 상태를 관찰한다.
void ALobbyGameState::AddPlayerState(APlayerState* PlayerState)
{
	Super::AddPlayerState(PlayerState);
	if (ALobbyPlayerState* LobbyPlayerState = Cast<ALobbyPlayerState>(PlayerState))
	{
		ULobbyPlayerStateComponent* LobbyState = LobbyPlayerState->GetLobbyPlayerStateComponent();
		LobbyState->OnLobbyRuntimeStateChanged.RemoveAll(this);
		LobbyState->OnLobbyRuntimeStateChanged.AddUObject(this, &ThisClass::NotifyLobbyStateChanged);
	}
	NotifyLobbyStateChanged();
}

// 참가자 제거가 완료된 목록을 HUD가 다시 읽도록 알린다.
void ALobbyGameState::RemovePlayerState(APlayerState* PlayerState)
{
	if (ALobbyPlayerState* LobbyPlayerState = Cast<ALobbyPlayerState>(PlayerState))
	{
		LobbyPlayerState->GetLobbyPlayerStateComponent()->OnLobbyRuntimeStateChanged.RemoveAll(this);
	}
	Super::RemovePlayerState(PlayerState);
	NotifyLobbyStateChanged();
}

void ALobbyGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(ALobbyGameState, bExperienceLoadFailed, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(ALobbyGameState, SelectedMapOption, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(ALobbyGameState, bStartPending, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(ALobbyGameState, GameStartEndServerTimeSeconds, Params);
}

void ALobbyGameState::SetSelectedMapOption(const FLobbyMatchMapOption& InMapOption)
{
	if (!HasAuthority())
	{
		return;
	}

	if (SelectedMapOption.MapKey == InMapOption.MapKey
		&& SelectedMapOption.DisplayName.EqualTo(InMapOption.DisplayName)
		&& SelectedMapOption.Map.ToSoftObjectPath() == InMapOption.Map.ToSoftObjectPath()
		&& SelectedMapOption.TravelMapName == InMapOption.TravelMapName
		&& SelectedMapOption.MaxPlayerCount == InMapOption.MaxPlayerCount
		&& SelectedMapOption.Thumbnail.Get() == InMapOption.Thumbnail.Get())
	{
		return;
	}

	SelectedMapOption = InMapOption;
	MARK_PROPERTY_DIRTY_FROM_NAME(ALobbyGameState, SelectedMapOption, this);
	ForceNetUpdate();

	NotifyLobbyStateChanged();
}

FLobbyMatchMapOption ALobbyGameState::GetSelectedMapOption() const
{
	FLobbyMatchMapOption ResolvedMapOption = SelectedMapOption;
	if (ResolvedMapOption.MapKey.IsNone()
		|| IsValid(ResolvedMapOption.Thumbnail))
	{
		return ResolvedMapOption;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	const ULobbyRuntimeSubsystem* LobbyRuntimeSubsystem =
		GameInstance
			? GameInstance->GetSubsystem<ULobbyRuntimeSubsystem>()
			: nullptr;
	const ULevelDefinition* LevelDefinition =
		LobbyRuntimeSubsystem
			? LobbyRuntimeSubsystem->GetLoadedLevelDefinition()
			: nullptr;
	FLobbyMatchMapOption ConfiguredMapOption;
	if (LevelDefinition
		&& LevelDefinition->FindIngameLevel(
			ResolvedMapOption.MapKey,
			ConfiguredMapOption))
	{
		ResolvedMapOption.Thumbnail = ConfiguredMapOption.Thumbnail;
	}

	return ResolvedMapOption;
}

bool ALobbyGameState::IsSelectedMapImageReady() const
{
	const FLobbyMatchMapOption ResolvedMapOption = GetSelectedMapOption();
	return !ResolvedMapOption.MapKey.IsNone()
		&& IsValid(ResolvedMapOption.Thumbnail);
}

void ALobbyGameState::SetGameStartPending(
	const bool bInStartPending,
	const double InStartEndServerTimeSeconds)
{
	if (!HasAuthority())
	{
		return;
	}

	const double NewEndServerTimeSeconds = bInStartPending
		? FMath::Max(InStartEndServerTimeSeconds, GetServerWorldTimeSeconds())
		: 0.0;
	const bool bPendingChanged = bStartPending != bInStartPending;
	const bool bEndTimeChanged = !FMath::IsNearlyEqual(
		GameStartEndServerTimeSeconds,
		NewEndServerTimeSeconds);
	if (!bPendingChanged && !bEndTimeChanged)
	{
		return;
	}

	bStartPending = bInStartPending;
	GameStartEndServerTimeSeconds = NewEndServerTimeSeconds;
	if (bPendingChanged)
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(ALobbyGameState, bStartPending, this);
	}
	if (bEndTimeChanged)
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(
			ALobbyGameState,
			GameStartEndServerTimeSeconds,
			this);
	}
	ForceNetUpdate();
	RefreshGameEntryContentPreload();
	NotifyLobbyStateChanged();
}

float ALobbyGameState::GetGameStartRemainingSeconds() const
{
	if (!bStartPending)
	{
		return 0.0f;
	}

	return static_cast<float>(FMath::Max(
		GameStartEndServerTimeSeconds - GetServerWorldTimeSeconds(),
		0.0));
}

void ALobbyGameState::OnRep_SelectedMapOption()
{

	NotifyLobbyStateChanged();
}

void ALobbyGameState::OnRep_GameStartState()
{
	RefreshGameEntryContentPreload();
	NotifyLobbyStateChanged();
}

void ALobbyGameState::RefreshGameEntryContentPreload() const
{
	UGameInstance* GameInstance = GetGameInstance();
	ULobbyRuntimeSubsystem* LobbyRuntimeSubsystem =
		GameInstance
			? GameInstance->GetSubsystem<ULobbyRuntimeSubsystem>()
			: nullptr;
	if (!LobbyRuntimeSubsystem)
	{
		return;
	}

	if (bStartPending)
	{
		LobbyRuntimeSubsystem->BeginGameEntryContentPreload();
	}
	else
	{
		LobbyRuntimeSubsystem->CancelGameEntryContentPreload();
	}
}

// 서버의 실패 여부는 늦게 입장한 클라이언트에도 상태로 전달한다.
void ALobbyGameState::SetExperienceLoadFailed(const bool bFailed)
{
	if (!HasAuthority() || bExperienceLoadFailed == bFailed)
	{
		return;
	}
	bExperienceLoadFailed = bFailed;
	MARK_PROPERTY_DIRTY_FROM_NAME(ALobbyGameState, bExperienceLoadFailed, this);
	ForceNetUpdate();
	NotifyLobbyStateChanged();
}

bool ALobbyGameState::HasExperienceLoadFailed() const
{
	return bExperienceLoadFailed || ExperienceManagerComponent->HasExperienceLoadFailed();
}

void ALobbyGameState::OnRep_ExperienceLoadFailed()
{
	NotifyLobbyStateChanged();
}

void ALobbyGameState::HandleExperienceLoaded(const UExperienceDefinition* Experience)
{
	NotifyLobbyStateChanged();
}

void ALobbyGameState::HandleExperienceLoadFailed(FPrimaryAssetId ExperienceId, const FString& FailureMessage)
{
	NotifyLobbyStateChanged();
}

void ALobbyGameState::NotifyLobbyStateChanged()
{
	OnLobbyStateChanged.Broadcast();
}
