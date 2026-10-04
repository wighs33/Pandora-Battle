#include "UI/Common/ButtonClickBinding.h"

#include "Components/Button.h"

void PdButtonClick::Bind(UObject* Handler, const TConstArrayView<FPdButtonClickBinding> Bindings)
{
	for (const FPdButtonClickBinding& Binding : Bindings)
	{
		if (Binding.Button)
		{
			FOnButtonClickedEvent::FDelegate Delegate;
			Delegate.BindUFunction(Handler, Binding.HandlerName);
			Binding.Button->OnClicked.AddUnique(Delegate);
		}
	}
}

void PdButtonClick::Unbind(UObject* Handler, const TConstArrayView<FPdButtonClickBinding> Bindings)
{
	for (const FPdButtonClickBinding& Binding : Bindings)
	{
		if (Binding.Button)
		{
			Binding.Button->OnClicked.Remove(Handler, Binding.HandlerName);
		}
	}
}
