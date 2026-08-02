#include "Settings/CursorSettingsLibrary.h"

#include "GameFramework/PlayerController.h"
#include "Settings/LocalPlayerSettingsSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CursorSettingsLibrary)

bool UCursorSettingsLibrary::ApplyConfiguredMouseCursor(UObject* WorldContextObject, APlayerController* PlayerController)
{
	static_cast<void>(WorldContextObject);

	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return false;
	}

	if (ULocalPlayerSettingsSubsystem* LocalPlayerSettings = ULocalPlayerSettingsSubsystem::Get(PlayerController))
	{
		return LocalPlayerSettings->ApplyConfiguredMouseCursor(PlayerController);
	}

	return false;
}
