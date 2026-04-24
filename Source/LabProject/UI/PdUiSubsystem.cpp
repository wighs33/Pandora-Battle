#include "UI/PdUiSubsystem.h"

#include "AbilitySystemComponent.h"
#include "Blueprint/UserWidget.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Mode/PdPlayerState.h"
#include "View/MVVMView.h"
#include "ViewModel/StatusViewModel.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(PdUiSubsystem)

DEFINE_LOG_CATEGORY(PdUiSubsystemLog);

void UPdUiSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	StatusViewModel = NewObject<UStatusViewModel>(this);
}

void UPdUiSubsystem::Deinitialize()
{
	BindStatusViewModelToASC(nullptr);
	StatusViewModel = nullptr;

	Super::Deinitialize();
}

void UPdUiSubsystem::RefreshStatusViewModel()
{
	BindStatusViewModelToASC(ResolveAbilitySystemComponent());
}

void UPdUiSubsystem::ApplyStatusViewModelToWidget(UUserWidget* InWidget)
{
	if (!InWidget || !StatusViewModel)
	{
		return;
	}

	RefreshStatusViewModel();

	UMVVMView* View = InWidget->GetExtension<UMVVMView>();
	if (!View)
	{
		UE_LOG(PdUiSubsystemLog, Warning, TEXT("ApplyStatusViewModelToWidget failed: widget '%s' does not have an MVVMView extension."), *GetNameSafe(InWidget));
		return;
	}

	const bool bSuccess = View->SetViewModel(UStatusViewModel::ViewModelName, StatusViewModel);
	if (!bSuccess)
	{
		UE_LOG(PdUiSubsystemLog, Warning, TEXT("ApplyStatusViewModelToWidget failed: could not set viewmodel '%s' on widget '%s'."),
			*UStatusViewModel::ViewModelName.ToString(),
			*GetNameSafe(InWidget));
	}
}

UAbilitySystemComponent* UPdUiSubsystem::ResolveAbilitySystemComponent() const
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

void UPdUiSubsystem::BindStatusViewModelToASC(UAbilitySystemComponent* InASC)
{
	if (!StatusViewModel)
	{
		return;
	}

	if (BoundAbilitySystemComponent.Get() == InASC)
	{
		if (InASC && StatusViewModel->IsViewModelInitialized())
		{
			StatusViewModel->UpdateAllData();
		}
		return;
	}

	if (StatusViewModel->IsViewModelInitialized())
	{
		StatusViewModel->UninitializeViewModel();
	}

	BoundAbilitySystemComponent = InASC;

	if (InASC)
	{
		StatusViewModel->InitializeViewModel(InASC);
	}
}
