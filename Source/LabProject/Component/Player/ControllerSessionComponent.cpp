#include "Component/Player/ControllerSessionComponent.h"

#include "Engine/World.h"
#include "Mode/ExperienceGameMode.h"
#include "Mode/ExperienceGameState.h"
#include "Engine/GameInstance.h"
#include "Lobby/LobbyRuntimeSubsystem.h"
#include "Mode/PdPlayerController.h"
#include "Online/OnlineSessionsSubsystem.h"
#include "Common/GameResultTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ControllerSessionComponent)

UControllerSessionComponent::UControllerSessionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

bool UControllerSessionComponent::RequestExitMatchToTitle()
{
	APdPlayerController* Controller = GetPdController();
	if (!Controller || !CanRequestExitMatchToTitle())
	{
		return false;
	}
	if (!Controller->HasAuthority() || !Controller->IsLocalController())
	{
		// false를 돌려주면 로컬 메뉴가 세션을 없애고 이동한다.
		// 서버는 실제 Logout을 본 뒤에만 경기를 정산한다.
		return false;
	}

	if (AExperienceGameMode* ExperienceGameMode =
		Controller->GetWorld()
			? Controller->GetWorld()->GetAuthGameMode<AExperienceGameMode>()
			: nullptr)
	{
		return ExperienceGameMode->RequestAbortMatchToTitle(Controller);
	}

	return false;
}

void UControllerSessionComponent::TravelToTitleWithGameResult(const FGameResultPresentationData& GameResultData,
	const FString& TitleMapName) const
{
	APdPlayerController* Controller = GetPdController();
	if (!Controller)
	{
		return;
	}

	if (ULobbyRuntimeSubsystem* LobbySubsystem = UGameInstance::GetSubsystem<ULobbyRuntimeSubsystem>(Controller->GetGameInstance()))
	{
		LobbySubsystem->SetPendingTitleGameResult(GameResultData);
	}

	DestroySessionAndTravelToTitle(TitleMapName);
}

void UControllerSessionComponent::TravelToTitleWithoutGameResult(const FString& TitleMapName) const
{
	APdPlayerController* Controller = GetPdController();
	if (!Controller)
	{
		return;
	}

	if (ULobbyRuntimeSubsystem* LobbySubsystem =
		UGameInstance::GetSubsystem<ULobbyRuntimeSubsystem>(Controller->GetGameInstance()))
	{
		LobbySubsystem->ClearPendingTitleGameResult();
	}
	if (UOnlineSessionsSubsystem* OnlineSessionsSubsystem =
		Controller->GetGameInstance()
			? Controller->GetGameInstance()
				->GetSubsystem<UOnlineSessionsSubsystem>()
			: nullptr)
	{
		OnlineSessionsSubsystem->MarkVoluntaryMatchExit();
	}

	DestroySessionAndTravelToTitle(TitleMapName);
}

void UControllerSessionComponent::DestroySessionAndTravelToTitle(const FString& TitleMapName) const
{
	APdPlayerController* Controller = GetPdController();
	if (!Controller)
	{
		return;
	}

	UOnlineSessionsSubsystem* OnlineSessionsSubsystem =
		Controller->GetGameInstance()
			? Controller->GetGameInstance()->GetSubsystem<UOnlineSessionsSubsystem>()
			: nullptr;
	if (OnlineSessionsSubsystem && OnlineSessionsSubsystem->HasNamedSession())
	{
		OnlineSessionsSubsystem->DestroySession();
	}

	if (!TitleMapName.IsEmpty())
	{
		Controller->ClientTravel(TitleMapName, TRAVEL_Absolute);
	}
}

APdPlayerController* UControllerSessionComponent::GetPdController() const
{
	return Cast<APdPlayerController>(GetOwner());
}

bool UControllerSessionComponent::CanRequestExitMatchToTitle() const
{
	const APdPlayerController* Controller = GetPdController();
	const UWorld* World = Controller ? Controller->GetWorld() : nullptr;
	if (!World || World->GetNetMode() == NM_Standalone)
	{
		return false;
	}

	const AExperienceGameState* ExperienceGameState = World->GetGameState<AExperienceGameState>();
	return ExperienceGameState && ExperienceGameState->PlayerArray.Num() > 1;
}
