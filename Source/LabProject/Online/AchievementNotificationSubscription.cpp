#include "Online/AchievementNotificationSubscription.h"

#include "Engine/GameInstance.h"
#include "Online/AchievementSubsystem.h"

UAchievementSubsystem* FAchievementNotificationSubscription::Subscribe(
	UGameInstance* GameInstance, const FSimpleDelegate& OnChanged, const bool bIncludePresentationReady)
{
	Reset();

	UAchievementSubsystem* AchievementSubsystem = GameInstance
		? GameInstance->GetSubsystem<UAchievementSubsystem>()
		: nullptr;
	if (!AchievementSubsystem)
	{
		return nullptr;
	}

	Subsystem = AchievementSubsystem;
	StateChangedHandle = AchievementSubsystem->OnSteamAchievementStateChanged().Add(OnChanged);
	if (bIncludePresentationReady)
	{
		PresentationReadyHandle = AchievementSubsystem->OnAchievementPresentationReady().Add(OnChanged);
	}
	return AchievementSubsystem;
}

void FAchievementNotificationSubscription::Reset()
{
	if (UAchievementSubsystem* AchievementSubsystem = Subsystem.Get())
	{
		AchievementSubsystem->OnSteamAchievementStateChanged().Remove(StateChangedHandle);
		AchievementSubsystem->OnAchievementPresentationReady().Remove(PresentationReadyHandle);
	}

	Subsystem.Reset();
	StateChangedHandle.Reset();
	PresentationReadyHandle.Reset();
}
