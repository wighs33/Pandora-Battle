#include "Chat/ChatEntryWidget.h"

#include "Components/TextBlock.h"
#include "UI/WidgetLookup.h"

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
	if (UTextBlock* MessageTextBlock = GetMessageTextBlock())
	{
		MessageTextBlock->SetText(FText::FromString(Message));
	}
}

UTextBlock* UChatEntryWidget::GetMessageTextBlock() const
{
	if (Txt_Message)
	{
		return Txt_Message.Get();
	}

	UWidgetTree* CurrentWidgetTree = WidgetTree;
	if (UTextBlock* MessageTextBlock = PdWidgetLookup::FindWidgetByNames<UTextBlock>(
		CurrentWidgetTree,
		MessageTextCandidateNames))
	{
		return MessageTextBlock;
	}

	return PdWidgetLookup::FindFirstWidgetOfType<UTextBlock>(CurrentWidgetTree);
}
