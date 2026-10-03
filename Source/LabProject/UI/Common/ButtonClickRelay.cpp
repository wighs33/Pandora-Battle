#include "UI/Common/ButtonClickRelay.h"

#include "Components/Button.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ButtonClickRelay)

void UButtonClickRelay::Bind(UButton* InButton, FSimpleDelegate InOnClicked)
{
	Unbind();
	Button = InButton;
	OnClicked = MoveTemp(InOnClicked);
	if (InButton)
	{
		InButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleClicked);
	}
}

void UButtonClickRelay::Unbind()
{
	if (UButton* BoundButton = Button.Get())
	{
		BoundButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleClicked);
	}
	Button.Reset();
	OnClicked.Unbind();
}

void UButtonClickRelay::HandleClicked()
{
	OnClicked.ExecuteIfBound();
}
