#include "Room/RoomGameMode.h"

#include "Room/RoomHUD.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RoomGameMode)

ARoomGameMode::ARoomGameMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	HUDClass = ARoomHUD::StaticClass();
	DefaultPawnClass = nullptr;
}
