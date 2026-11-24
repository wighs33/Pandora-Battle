#include "UI/Story/StoryHUD.h"

#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Settings/LocalPlayerSettingsSubsystem.h"
#include "Story/StoryDirector.h"
#include "UI/Core/LoadingScreenSubsystem.h"
#include "UI/Core/UiSubsystem.h"
#include "UI/Story/StoryPauseMenu.h"
#include "UI/Story/StoryScreen.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StoryHUD)

AStoryHUD::AStoryHUD(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
}

void AStoryHUD::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PlayerController = GetOwningPlayerController();
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return;
	}
	StoryScreen = UStoryScreen::Create(PlayerController, FSimpleDelegate::CreateUObject(this, &ThisClass::OpenPauseMenu));
	PlayerController->GetLocalPlayer()->GetSubsystem<UUiSubsystem>()->PushScreen(StoryScreen, EUiScreenLayer::Screen);
	// 엔진 기본 PlayerController를 사용하므로 로컬 설정을 여기서 적용한다.
	PlayerController->GetLocalPlayer()->GetSubsystem<ULocalPlayerSettingsSubsystem>()->ApplyLocalPlayerSettings(PlayerController);
}

// 연결 대기 팝업이 닫힌 뒤에 첫 단계를 시작한다. 팝업 뒤에서 대사가 지나가지 않게 한다.
void AStoryHUD::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bChapterStarted || !StoryScreen)
	{
		return;
	}
	const ULocalPlayer* LocalPlayer = GetOwningPlayerController()->GetLocalPlayer();
	const ULoadingScreenSubsystem* Loading = LocalPlayer ? LocalPlayer->GetSubsystem<ULoadingScreenSubsystem>() : nullptr;
	if (Loading && Loading->HasBlockingWait())
	{
		return;
	}
	bChapterStarted = true;
	if (AStoryDirector* Director = AStoryDirector::Find(this))
	{
		Director->BeginChapter(*StoryScreen);
	}
}

void AStoryHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (PauseMenu)
	{
		PauseMenu->DeactivateWidget();
		PauseMenu = nullptr;
	}
	if (StoryScreen)
	{
		StoryScreen->DeactivateWidget();
		StoryScreen = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void AStoryHUD::OpenPauseMenu()
{
	if (PauseMenu)
	{
		return;
	}
	APlayerController* PlayerController = GetOwningPlayerController();
	PauseMenu = UStoryPauseMenu::Create(PlayerController, FSimpleDelegate::CreateUObject(this, &ThisClass::HandlePauseMenuClosed));
	PlayerController->SetPause(true);
	PlayerController->GetLocalPlayer()->GetSubsystem<UUiSubsystem>()->PushScreen(PauseMenu, EUiScreenLayer::Menu);
}

void AStoryHUD::HandlePauseMenuClosed()
{
	PauseMenu = nullptr;
	if (APlayerController* PlayerController = GetOwningPlayerController())
	{
		PlayerController->SetPause(false);
	}
}
