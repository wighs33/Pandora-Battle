#include "Lobby/Contents/TitleGameMode.h"

#include "Lobby/Contents/TitleHUD.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TitleGameMode)

ATitleGameMode::ATitleGameMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	HUDClass = ATitleHUD::StaticClass();
	DefaultPawnClass = nullptr;
}
