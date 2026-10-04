#include "UI/Chat/ChatBoxWidget.h"

#include "Component/Chat/ChatControllerComponent.h"
#include "UI/Chat/ChatEntryWidget.h"
#include "Components/EditableText.h"
#include "Components/ScrollBox.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "UI/Core/PdUIActionRouter.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ChatBoxWidget)

DEFINE_LOG_CATEGORY_STATIC(LogChatBoxWidget, Log, All);

void UChatBoxWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (TxtBox_ChatInput)
	{
		TxtBox_ChatInput->OnTextCommitted.RemoveDynamic(this, &ThisClass::HandleChatTextCommitted);
		TxtBox_ChatInput->OnTextCommitted.AddUniqueDynamic(this, &ThisClass::HandleChatTextCommitted);
	}
	SetChatInputEnabled(false);

	// 채팅 메시지와 입력 명령은 소유 컨트롤러의 채팅 컴포넌트가 알린다.
	const APlayerController* PlayerController = GetOwningPlayer();
	ChatControllerComponent = PlayerController ? PlayerController->FindComponentByClass<UChatControllerComponent>() : nullptr;
	if (ChatControllerComponent)
	{
		ChatMessageAddedHandle = ChatControllerComponent->OnChatMessageAdded().AddUObject(this, &ThisClass::AddChatMessage);
		ChatViewCommandHandle = ChatControllerComponent->OnChatViewCommand().AddUObject(this, &ThisClass::HandleChatViewCommand);
	}
}

void UChatBoxWidget::NativeDestruct()
{
	if (TxtBox_ChatInput)
	{
		TxtBox_ChatInput->OnTextCommitted.RemoveDynamic(this, &ThisClass::HandleChatTextCommitted);
	}

	// LocalPlayer가 남아 있어도 종료 중에는 서브시스템이 먼저 해제될 수 있다.
	if (UPdUIActionRouter* Router = ULocalPlayer::GetSubsystem<UPdUIActionRouter>(GetOwningLocalPlayer()))
	{
		Router->EndChatInput(GetChatInputWidget());
	}
	SetChatFocused(false);
	if (ChatControllerComponent)
	{
		ChatControllerComponent->OnChatMessageAdded().Remove(ChatMessageAddedHandle);
		ChatControllerComponent->OnChatViewCommand().Remove(ChatViewCommandHandle);
		ChatControllerComponent = nullptr;
	}
	ChatMessageAddedHandle.Reset();
	ChatViewCommandHandle.Reset();
	Super::NativeDestruct();
}

FReply UChatBoxWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (bChatFocused && InKeyEvent.GetKey() == EKeys::Escape)
	{
		ExitChat();
		return FReply::Handled();
	}

	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

void UChatBoxWidget::FocusChat()
{
	APlayerController* PlayerController = GetOwningPlayer();
	UEditableText* ChatInputText = GetChatInputWidget();
	UPdUIActionRouter* Router = ULocalPlayer::GetSubsystem<UPdUIActionRouter>(GetOwningLocalPlayer());
	if (!PlayerController || !ChatInputText || !Router)
	{
		return;
	}

	SetChatInputEnabled(true);
	SetChatFocused(true);

	Router->BeginChatInput(ChatInputText);
}

void UChatBoxWidget::ExitChat()
{
	SetChatFocused(false);
	SetChatInputText(FText::GetEmpty());
	SetChatInputEnabled(false);

	if (UPdUIActionRouter* Router = ULocalPlayer::GetSubsystem<UPdUIActionRouter>(GetOwningLocalPlayer()))
	{
		Router->EndChatInput(GetChatInputWidget());
	}
}

void UChatBoxWidget::Scroll(const bool bUp)
{
	UScrollBox* ChatScrollBox = GetChatScrollBox();
	if (!ChatScrollBox)
	{
		return;
	}

	const float Direction = bUp ? -1.0f : 1.0f;
	const float CurrentOffset = ChatScrollBox->GetScrollOffset();
	const float MaxOffset = FMath::Max(0.0f, ChatScrollBox->GetScrollOffsetOfEnd());
	const float NewOffset = FMath::Clamp(CurrentOffset + Direction * ScrollMultiplier, 0.0f, MaxOffset);
	ChatScrollBox->SetScrollOffset(NewOffset);
}

void UChatBoxWidget::SubmitChatInput()
{
	if (ChatControllerComponent)
	{
		ChatControllerComponent->SubmitChatMessage(GetChatInputText());
	}

	ExitChat();
}

void UChatBoxWidget::AddChatMessage(const FString& Message)
{
	UScrollBox* ChatScrollBox = GetChatScrollBox();
	if (!ChatScrollBox)
	{
		return;
	}

	if (!ChatEntryWidgetClass)
	{
		UE_LOG(LogChatBoxWidget, Warning, TEXT("%s has no ChatEntryWidgetClass, so chat messages are dropped."), *GetNameSafe(GetClass()));
		return;
	}

	UChatEntryWidget* EntryWidget = CreateWidget<UChatEntryWidget>(GetOwningPlayer(), ChatEntryWidgetClass);
	if (!EntryWidget)
	{
		return;
	}

	EntryWidget->SetMessage(Message);
	ChatScrollBox->AddChild(EntryWidget);
	ChatScrollBox->ScrollToEnd();
}

void UChatBoxWidget::HandleChatTextCommitted(const FText& Text, const ETextCommit::Type CommitMethod)
{
	static_cast<void>(Text);

	if (CommitMethod == ETextCommit::OnEnter)
	{
		SubmitChatInput();
	}
}

void UChatBoxWidget::HandleChatViewCommand(const EChatViewCommand Command)
{
	switch (Command)
	{
	case EChatViewCommand::Focus:
		FocusChat();
		break;
	case EChatViewCommand::Exit:
		ExitChat();
		break;
	case EChatViewCommand::ScrollUp:
		Scroll(true);
		break;
	case EChatViewCommand::ScrollDown:
		Scroll(false);
		break;
	case EChatViewCommand::SubmitInput:
		SubmitChatInput();
		break;
	}
}

// 입력 처리는 채팅 컴포넌트가 포커스 여부로 갈라 보내므로, 포커스가 바뀔 때마다 알려 준다.
void UChatBoxWidget::SetChatFocused(const bool bFocused)
{
	bChatFocused = bFocused;
	if (ChatControllerComponent)
	{
		ChatControllerComponent->SetChatFocused(bFocused);
	}
}

void UChatBoxWidget::OnMenuLanguageChanged()
{
	SetChatInputEnabled(bChatFocused);
}

void UChatBoxWidget::SetChatInputEnabled(const bool bEnabled) const
{
	if (UEditableText* ChatInputText = GetChatInputWidget())
	{
		ChatInputText->SetIsEnabled(bEnabled);
		ChatInputText->SetHintText(bEnabled ? MenuText(TEXT("HUD.ChatActive")) : MenuText(TEXT("HUD.ChatIdle")));
	}
}

FString UChatBoxWidget::GetChatInputText() const
{
	if (const UEditableText* ChatInputText = GetChatInputWidget())
	{
		return ChatInputText->GetText().ToString();
	}

	return FString();
}

void UChatBoxWidget::SetChatInputText(const FText& Text) const
{
	if (UEditableText* ChatInputText = GetChatInputWidget())
	{
		ChatInputText->SetText(Text);
	}
}

UEditableText* UChatBoxWidget::GetChatInputWidget() const
{
	return TxtBox_ChatInput;
}

UScrollBox* UChatBoxWidget::GetChatScrollBox() const
{
	return ScrollBox_ChatMessages;
}
