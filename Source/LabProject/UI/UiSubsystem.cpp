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

/** 서브시스템을 초기화합니다. */
void UUiSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// =================================================================================================================
	// === ViewModel 생성

	StatusViewModel = NewObject<UStatusViewModel>(this);
}

/** 서브시스템을 정리합니다. */
void UUiSubsystem::Deinitialize()
{
	// =================================================================================================================
	// === 바인딩 해제

	BindStatusViewModelToASC(nullptr);
	StatusViewModel = nullptr;

	Super::Deinitialize();
}

/** 상태창 ViewModel을 새로 고칩니다. */
void UUiSubsystem::RefreshStatusViewModel()
{
	BindStatusViewModelToASC(ResolveAbilitySystemComponent());
}

/** 위젯에 상태창 ViewModel을 적용합니다. */
void UUiSubsystem::ApplyStatusViewModelToWidget(UUserWidget* InWidget)
{
	// =================================================================================================================
	// === 초기화 & 안전 가드

	if (!InWidget || !StatusViewModel)
	{
		return;
	}

	RefreshStatusViewModel();

	// =================================================================================================================
	// === MVVMView 조회

	UMVVMView* View = InWidget->GetExtension<UMVVMView>();
	if (!View)
	{
		UE_LOG(PdUiSubsystemLog, Warning, TEXT("ApplyStatusViewModelToWidget failed: widget '%s' does not have an MVVMView extension."), *GetNameSafe(InWidget));
		return;
	}

	// =================================================================================================================
	// === ViewModel 적용

	const bool bSuccess = View->SetViewModel(UStatusViewModel::ViewModelName, StatusViewModel);
	if (!bSuccess)
	{
		UE_LOG(PdUiSubsystemLog, Warning, TEXT("ApplyStatusViewModelToWidget failed: could not set viewmodel '%s' on widget '%s'."),
			*UStatusViewModel::ViewModelName.ToString(),
			*GetNameSafe(InWidget));
	}
}

/** 현재 로컬 플레이어의 ASC를 찾습니다. */
UAbilitySystemComponent* UUiSubsystem::ResolveAbilitySystemComponent() const
{
	// =================================================================================================================
	// === 로컬 플레이어 조회

	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer)
	{
		return nullptr;
	}

	// =================================================================================================================
	// === 로컬 컨트롤러 조회

	const APlayerController* PlayerController = LocalPlayer->GetPlayerController(GetWorld());
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return nullptr;
	}

	// =================================================================================================================
	// === PlayerState 기준 ASC 조회

	const APdPlayerState* PdPlayerState = PlayerController->GetPlayerState<APdPlayerState>();
	return PdPlayerState ? PdPlayerState->GetAbilitySystemComponent() : nullptr;
}

/** 상태창 ViewModel을 ASC에 바인딩합니다. */
void UUiSubsystem::BindStatusViewModelToASC(UAbilitySystemComponent* InASC)
{
	// =================================================================================================================
	// === 초기화 & 안전 가드

	if (!StatusViewModel)
	{
		return;
	}

	// =================================================================================================================
	// === 같은 ASC 재사용

	if (BoundAbilitySystemComponent.Get() == InASC)
	{
		if (InASC && StatusViewModel->IsViewModelInitialized())
		{
			StatusViewModel->UpdateAllData();
		}
		return;
	}

	// =================================================================================================================
	// === 기존 바인딩 해제

	if (StatusViewModel->IsViewModelInitialized())
	{
		StatusViewModel->UninitializeViewModel();
	}

	BoundAbilitySystemComponent = InASC;

	// =================================================================================================================
	// === 새 ASC 바인딩

	if (InASC)
	{
		StatusViewModel->InitializeViewModel(InASC);
	}
}