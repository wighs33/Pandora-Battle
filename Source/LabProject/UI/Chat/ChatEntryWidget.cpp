#include "UI/Chat/ChatEntryWidget.h"

#include "Components/TextBlock.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ChatEntryWidget)

void UChatEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshUI();
}

void UChatEntryWidget::SetMessage(const FString& InMessage)
{
	Message = InMessage;
	RefreshUI();
}

void UChatEntryWidget::RefreshUI()
{
	if (Txt_Message)
	{
		Txt_Message->SetText(FText::FromString(Message));
	}
}
