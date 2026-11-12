#include "UI/Common/ButtonClickRelay.h"

#include "Components/Button.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ButtonClickRelay)

void UButtonClickRelay::Bind(UButton* InButton, FSimpleDelegate InOnClicked, FOnHoverChanged InOnHoverChanged)
{
	Unbind();
	Button = InButton;
	OnClicked = MoveTemp(InOnClicked);
	OnHoverChanged = MoveTemp(InOnHoverChanged);
	if (!InButton)
	{
		return;
	}
	InButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleClicked);
	if (OnHoverChanged.IsBound())
	{
		InButton->OnHovered.AddUniqueDynamic(this, &ThisClass::HandleHovered);
		InButton->OnUnhovered.AddUniqueDynamic(this, &ThisClass::HandleUnhovered);
	}
}

void UButtonClickRelay::Unbind()
{
	if (UButton* BoundButton = Button.Get())
	{
		BoundButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleClicked);
		BoundButton->OnHovered.RemoveDynamic(this, &ThisClass::HandleHovered);
		BoundButton->OnUnhovered.RemoveDynamic(this, &ThisClass::HandleUnhovered);
	}
	Button.Reset();
	OnClicked.Unbind();
	OnHoverChanged.Unbind();
}

void UButtonClickRelay::HandleClicked()
{
	OnClicked.ExecuteIfBound();
}

void UButtonClickRelay::HandleHovered()
{
	OnHoverChanged.ExecuteIfBound(true);
}

void UButtonClickRelay::HandleUnhovered()
{
	OnHoverChanged.ExecuteIfBound(false);
}
