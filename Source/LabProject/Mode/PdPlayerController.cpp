#include "Mode/PdPlayerController.h"

#include "Components/GameFrameworkComponentManager.h"
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

UControllerInputComponent* APdPlayerController::GetControllerInputComponent() const
{
	return FindComponentByClass<UControllerInputComponent>();
}
