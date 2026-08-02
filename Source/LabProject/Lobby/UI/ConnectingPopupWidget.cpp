#include "Lobby/UI/ConnectingPopupWidget.h"

#include "Components/Button.h"
#include "Engine/LocalPlayer.h"
#include "UI/UiSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ConnectingPopupWidget)

UConnectingPopupWidget::UConnectingPopupWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UConnectingPopupWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsFocusable(true);
	ApplyCancelButtonState();
	PlayWaitAnimation();

	if (Btn_Cancel)
	{
		Btn_Cancel->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCancelClicked);
	}
}

void UConnectingPopupWidget::NativeDestruct()
{
	if (Btn_Cancel)
	{
		Btn_Cancel->OnClicked.RemoveDynamic(this, &ThisClass::HandleCancelClicked);
	}

	OnCanceled.Clear();
	if (const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
	{
		if (UUiSubsystem* UiSubsystem = LocalPlayer->GetSubsystem<UUiSubsystem>())
		{
			UiSubsystem->ReleaseModalInputsForOwner(this);
		}
	}

	StopWaitAnimation();
	Super::NativeDestruct();
}

void UConnectingPopupWidget::SetCancelButtonEnabled(const bool bEnabled)
{
	bCancelButtonEnabled = bEnabled;
	ApplyCancelButtonState();
}

void UConnectingPopupWidget::HandleCancelClicked()
{
	OnCanceled.Broadcast();
	OnCanceled.Clear();
	RemoveFromParent();
}

void UConnectingPopupWidget::ApplyCancelButtonState() const
{
	if (Btn_Cancel)
	{
		Btn_Cancel->SetVisibility(ESlateVisibility::Visible);
		Btn_Cancel->SetIsEnabled(bCancelButtonEnabled);
		Btn_Cancel->SetRenderOpacity(bCancelButtonEnabled ? 1.0f : 0.45f);
	}
}

void UConnectingPopupWidget::PlayWaitAnimation()
{
	if (!WaitAnimation)
	{
		return;
	}

	StopAnimation(WaitAnimation);
	PlayAnimation(WaitAnimation, 0.0f, 0, EUMGSequencePlayMode::Forward, WaitAnimationPlaybackSpeed);
}

void UConnectingPopupWidget::StopWaitAnimation()
{
	if (WaitAnimation)
	{
		StopAnimation(WaitAnimation);
	}
}
