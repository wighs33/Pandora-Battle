#include "UI/Common/ButtonClickBinding.h"

#include "Components/Button.h"

void ButtonClick::Bind(UObject* Handler, const TConstArrayView<FButtonClickBinding> Bindings)
{
	for (const FButtonClickBinding& Binding : Bindings)
	{
		if (Binding.Button)
		{
			FOnButtonClickedEvent::FDelegate Delegate;
			Delegate.BindUFunction(Handler, Binding.HandlerName);
			Binding.Button->OnClicked.AddUnique(Delegate);
		}
	}
}

void ButtonClick::Unbind(UObject* Handler, const TConstArrayView<FButtonClickBinding> Bindings)
{
	for (const FButtonClickBinding& Binding : Bindings)
	{
		if (Binding.Button)
		{
			Binding.Button->OnClicked.Remove(Handler, Binding.HandlerName);
		}
	}
}
