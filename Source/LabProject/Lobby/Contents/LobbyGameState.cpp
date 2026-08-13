#include "Lobby/Contents/LobbyGameState.h"

#include "Component/Experience/ExperienceManagerComponent.h"
#include "Engine/GameInstance.h"
#include "Lobby/Contents/LobbyHUD.h"
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

void ALobbyGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(ALobbyGameState, SelectedMapOption, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(ALobbyGameState, bStartPending, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(ALobbyGameState, GameStartEndServerTimeSeconds, Params);
}

void ALobbyGameState::SetSelectedMapOption(const FLobbyMatchMapOption& InMapOption)
{
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

RefreshLocalLobbyUI();
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
	RefreshLocalLobbyUI();
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

RefreshLocalLobbyUI();
}

void ALobbyGameState::OnRep_GameStartState()
{
	RefreshGameEntryContentPreload();
	RefreshLocalLobbyUI();
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

void ALobbyGameState::RefreshLocalLobbyUI() const
{
	const UWorld* World = GetWorld();
	APlayerController* LocalPlayerController = World ? World->GetFirstPlayerController() : nullptr;
	if (!LocalPlayerController || !LocalPlayerController->IsLocalController())
	{
		return;
	}

	if (ALobbyHUD* LobbyHUD = LocalPlayerController->GetHUD<ALobbyHUD>())
	{
		if (bStartPending)
		{
			LobbyHUD->CreateLobbyUI();
		}
		LobbyHUD->RefreshLobbyUI();
	}
}
