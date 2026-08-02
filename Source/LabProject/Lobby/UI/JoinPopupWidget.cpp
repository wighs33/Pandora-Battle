#include "Lobby/UI/JoinPopupWidget.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(JoinPopupWidget)

void UJoinPopupWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_Join)
	{
		Btn_Join->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleJoinClicked);
	}

	if (Btn_Cancel)
	{
		Btn_Cancel->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCancelClicked);
	}
}

void UJoinPopupWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ConnectingPopupTimerHandle);
	}

	if (Btn_Join)
	{
		Btn_Join->OnClicked.RemoveDynamic(this, &ThisClass::HandleJoinClicked);
	}

	if (Btn_Cancel)
	{
		Btn_Cancel->OnClicked.RemoveDynamic(this, &ThisClass::HandleCancelClicked);
	}

	Super::NativeDestruct();
}

void UJoinPopupWidget::HandleJoinClicked()
{
	const FString Address = Editable_InputIP ? Editable_InputIP->GetText().ToString().TrimStartAndEnd() : FString();
	if (Address.IsEmpty())
	{

		return;
	}

	if (ConnectingPopupWidgetClass)
	{
		ConnectingPopupWidget = CreateWidget<UUserWidget>(GetOwningPlayer(), ConnectingPopupWidgetClass);
		if (ConnectingPopupWidget)
		{
			ConnectingPopupWidget->AddToViewport(20);
		}

		if (UWorld* World = GetWorld(); World && ConnectingPopupLifetime > 0.0f)
		{
			World->GetTimerManager().SetTimer(
				ConnectingPopupTimerHandle,
				this,
				&ThisClass::RemoveConnectingPopup,
				ConnectingPopupLifetime,
				false);
		}
	}

	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		PlayerController->ClientTravel(Address, TRAVEL_Absolute);
	}
}

void UJoinPopupWidget::HandleCancelClicked()
{
	RemoveFromParent();
}

void UJoinPopupWidget::RemoveConnectingPopup()
{
	if (ConnectingPopupWidget)
	{
		ConnectingPopupWidget->RemoveFromParent();
		ConnectingPopupWidget = nullptr;
	}
}
