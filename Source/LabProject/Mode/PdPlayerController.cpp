#include "Mode/PdPlayerController.h"

#include "Components/GameFrameworkComponentManager.h"
#include "Mode/PdHUD.h"
#include "PlayerComponent/ControllerInputComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PdPlayerController)

DEFINE_LOG_CATEGORY(PdPlayerControllerLog);

APdPlayerController::APdPlayerController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void APdPlayerController::PreInitializeComponents()
{
	Super::PreInitializeComponents();
	UGameFrameworkComponentManager::AddGameFrameworkComponentReceiver(this);
}

void APdPlayerController::BeginPlay()
{
	Super::BeginPlay();
	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(this, UGameFrameworkComponentManager::NAME_GameActorReady);
}

void APdPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UGameFrameworkComponentManager::RemoveGameFrameworkComponentReceiver(this);
	Super::EndPlay(EndPlayReason);
}

void APdPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (UControllerInputComponent* ControllerInputComponent = GetControllerInputComponent())
	{
		ControllerInputComponent->RefreshInputDefinition();
	}
}

void APdPlayerController::Client_ShowRightNotification_Implementation(const FPdNotificationData& NotificationData)
{
	UE_LOG(PdPlayerControllerLog, Log,
		TEXT("[Notification] client rpc received. controller=%s hud=%s text=%s icon=%s"),
		*GetNameSafe(this),
		*GetNameSafe(GetHUD()),
		*NotificationData.Text.ToString(),
		*GetNameSafe(NotificationData.IconResource));

	if (APdHUD* PdHUD = GetHUD<APdHUD>())
	{
		PdHUD->ShowRightNotification(NotificationData);
	}
	else
	{
		UE_LOG(PdPlayerControllerLog, Warning,
			TEXT("[Notification] client rpc skipped: APdHUD missing. controller=%s hud=%s text=%s"),
			*GetNameSafe(this),
			*GetNameSafe(GetHUD()),
			*NotificationData.Text.ToString());
	}
}

UControllerInputComponent* APdPlayerController::GetControllerInputComponent() const
{
	return FindComponentByClass<UControllerInputComponent>();
}
