#include "Component/Player/ControllerSessionComponent.h"

#include "Engine/World.h"
#include "Mode/ExperienceGameMode.h"
#include "Mode/ExperienceGameState.h"
#include "Mode/PdGameInstance.h"
#include "Mode/PdPlayerController.h"
#include "Online/OnlineSessionsSubsystem.h"
#include "UI/GameResultTypes.h"

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

	if (AExperienceGameMode* ExperienceGameMode =
		Controller->GetWorld()
			? Controller->GetWorld()->GetAuthGameMode<AExperienceGameMode>()
			: nullptr)
	{
		return ExperienceGameMode->RequestAbortMatchToTitle(Controller);
	}

	Controller->Server_RequestExitMatchToTitle();
	return true;
}

void UControllerSessionComponent::HandleServerRequestExitMatchToTitle() const
{
	APdPlayerController* Controller = GetPdController();
	AExperienceGameMode* ExperienceGameMode =
		Controller && Controller->GetWorld()
			? Controller->GetWorld()->GetAuthGameMode<AExperienceGameMode>()
			: nullptr;
	if (ExperienceGameMode)
	{
		ExperienceGameMode->RequestAbortMatchToTitle(Controller);
	}
}

void UControllerSessionComponent::TravelToTitleWithGameResult(
	const FGameResultPresentationData& GameResultData,
	const FString& TitleMapName) const
{
	APdPlayerController* Controller = GetPdController();
	if (!Controller)
	{
		return;
	}

	if (UPdGameInstance* PdGameInstance = Controller->GetGameInstance<UPdGameInstance>())
	{
		PdGameInstance->SetPendingTitleGameResult(GameResultData);
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

	const AExperienceGameState* ExperienceGameState =
		World->GetGameState<AExperienceGameState>();
	return ExperienceGameState && ExperienceGameState->PlayerArray.Num() > 1;
}
