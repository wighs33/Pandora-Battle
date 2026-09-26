#include "Room/RoomHUD.h"
#include "UI/UiSubsystem.h"
#include "UI/UiScreen.h"
#include "Engine/LocalPlayer.h"

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

	Screen = CreateWidget<UUiScreen>(PlayerController);
	FUIInputConfig Config(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
	Config.bIgnoreMoveInput = Config.bIgnoreLookInput = true;
	// 타이틀/방 목록의 종료는 기존 버튼이 담당한다.
	Screen->SetContent(RoomListWidget, Config, RoomListWidget, FSimpleDelegate::CreateLambda([]() {}));
	PlayerController->GetLocalPlayer()->GetSubsystem<UUiSubsystem>()->PushScreen(Screen, EUiScreenLayer::Screen);
	UCursorSettingsLibrary::ApplyConfiguredMouseCursor(this, PlayerController);
}

void ARoomHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Screen) Screen->DeactivateWidget();
	Screen = nullptr;
	if (RoomListWidget)
	{
		RoomListWidget->RemoveFromParent();
		RoomListWidget = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}
