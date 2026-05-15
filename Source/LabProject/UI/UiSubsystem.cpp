#include "UI/UiSubsystem.h"

#include "AbilitySystemComponent.h"
#include "Blueprint/UserWidget.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Mode/PdPlayerState.h"
#include "View/MVVMView.h"
#include "ViewModel/StatusViewModel.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(UiSubsystem)

DEFINE_LOG_CATEGORY(PdUiSubsystemLog);

void UUiSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	StatusViewModel = NewObject<UStatusViewModel>(this);
}

void UUiSubsystem::Deinitialize()
{
	if (StatusViewModel && StatusViewModel->IsViewModelInitialized())
	{
		StatusViewModel->UninitializeViewModel();
	}

	StatusViewModel = nullptr;
	Super::Deinitialize();
}

void UUiSubsystem::RefreshStatusViewModel()
{
	if (!StatusViewModel)
	{
		return;
	}

	UAbilitySystemComponent* ASC = ResolveAbilitySystemComponent();
	if (!ASC)
	{
		if (StatusViewModel->IsViewModelInitialized())
		{
			StatusViewModel->UninitializeViewModel();
		}
		return;
	}

	StatusViewModel->InitializeViewModel(ASC);
}

void UUiSubsystem::ApplyStatusViewModelToWidget(UUserWidget* InWidget)
{
	if (!InWidget || !StatusViewModel)
	{
		return;
	}

	RefreshStatusViewModel();

	UMVVMView* ViewExtension = InWidget->GetExtension<UMVVMView>();
	if (!ViewExtension)
	{
		UE_LOG(PdUiSubsystemLog, Warning, TEXT("ApplyStatusViewModelToWidget failed: widget '%s' does not have an MVVMView extension."), *GetNameSafe(InWidget));
		return;
	}

	const bool bSuccess = ViewExtension->SetViewModel(UStatusViewModel::ViewModelName, StatusViewModel);
	if (!bSuccess)
	{
		UE_LOG(
			PdUiSubsystemLog,
			Warning,
			TEXT("ApplyStatusViewModelToWidget failed: could not set viewmodel '%s' on widget '%s'."),
			*UStatusViewModel::ViewModelName.ToString(),
			*GetNameSafe(InWidget));
		return;
	}

	StatusViewModel->UpdateAllData();
}

UAbilitySystemComponent* UUiSubsystem::ResolveAbilitySystemComponent() const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer)
	{
		return nullptr;
	}

	const APlayerController* PlayerController = LocalPlayer->GetPlayerController(GetWorld());
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return nullptr;
	}

	const APdPlayerState* PdPlayerState = PlayerController->GetPlayerState<APdPlayerState>();
	return PdPlayerState ? PdPlayerState->GetAbilitySystemComponent() : nullptr;
}
