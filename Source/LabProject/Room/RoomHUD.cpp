#include "Room/RoomHUD.h"
#include "UI/UiSubsystem.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Room/RoomListWidget.h"
#include "Settings/CursorSettingsLibrary.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RoomHUD)

void ARoomHUD::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PlayerController = GetOwningPlayerController();
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return;
	}

	if (!RoomListWidget && RoomListWidgetClass)
	{
		RoomListWidget = CreateWidget<URoomListWidget>(PlayerController, RoomListWidgetClass);
	}

	if (!RoomListWidget)
	{

		return;
	}

	RoomListWidget->AddToViewport();
	UUiSubsystem::SetBaseInputMode(PlayerController, EUiInputMode::UIOnly, nullptr);
	UCursorSettingsLibrary::ApplyConfiguredMouseCursor(this, PlayerController);
}

void ARoomHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (RoomListWidget)
	{
		RoomListWidget->RemoveFromParent();
		RoomListWidget = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}
