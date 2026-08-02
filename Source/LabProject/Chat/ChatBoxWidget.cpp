#include "Chat/ChatBoxWidget.h"

#include "Component/Chat/ChatControllerComponent.h"
#include "Chat/ChatEntryWidget.h"
#include "Components/EditableText.h"
#include "Components/ScrollBox.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "UI/UiSubsystem.h"
#include "UI/WidgetLookup.h"
#include "Definition/UI/WidgetClassDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ChatBoxWidget)

UChatBoxWidget::UChatBoxWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ChatScrollBoxCandidateNames =
	{
		TEXT("ScrollBox_ChatMessages"),
		TEXT("ScrollBox_MessageList"),
		TEXT("ChatScrollBox"),
		TEXT("MessageScrollBox"),
		TEXT("ScrollBox")
	};

	ChatInputCandidateNames =
	{
		TEXT("TxtBox_ChatInput"),
		TEXT("EditableText_ChatInput"),
		TEXT("Input_Chat"),
		TEXT("ChatInput"),
		TEXT("InputMessage"),
		TEXT("EditableTextBox"),
		TEXT("EditableText")
	};
}

void UChatBoxWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ApplyWidgetDefinitionSettings();
	CacheWidgets();
	SetChatInputEnabled(false);
}

void UChatBoxWidget::NativeDestruct()
{
	if (CachedChatInputText)
	{
		CachedChatInputText->OnTextCommitted.RemoveDynamic(this, &ThisClass::HandleChatTextCommitted);
	}

	ReleaseRoutedChatInput();
	bChatFocused = false;
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

void UChatBoxWidget::InitializeChat(
	UChatControllerComponent* InChatControllerComponent,
	TSubclassOf<UChatEntryWidget> InChatEntryWidgetClass,
	const float InScrollMultiplier)
{
	ChatControllerComponent = InChatControllerComponent;

	if (InChatEntryWidgetClass)
	{
		ChatEntryWidgetClass = InChatEntryWidgetClass;
	}

	ScrollMultiplier = InScrollMultiplier;
	ApplyWidgetDefinitionSettings();
	CacheWidgets();
	SetChatInputEnabled(false);
}

void UChatBoxWidget::FocusChat()
{
	APlayerController* PlayerController = GetOwningPlayer();
	UEditableText* ChatInputText = GetChatInputWidget();
	if (!PlayerController || !ChatInputText)
	{

		return;
	}

	SetChatInputEnabled(true);
	bChatFocused = true;

	if (!ApplyRoutedChatInput(ChatInputText))
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetWidgetToFocus(ChatInputText->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(true);
		PlayerController->SetInputMode(InputMode);
		PlayerController->bShowMouseCursor = false;
		PlayerController->bEnableClickEvents = false;
		PlayerController->bEnableMouseOverEvents = false;
	}

	ChatInputText->SetUserFocus(PlayerController);
	ChatInputText->SetKeyboardFocus();
}

void UChatBoxWidget::ExitChat()
{
	bChatFocused = false;
	SetChatInputText(FText::GetEmpty());
	SetChatInputEnabled(false);

	if (ReleaseRoutedChatInput())
	{
		return;
	}

	const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	UUiSubsystem* UiSubsystem = LocalPlayer
		? LocalPlayer->GetSubsystem<UUiSubsystem>()
		: nullptr;
	if (!UiSubsystem || !UiSubsystem->HasActiveModalInput())
	{
		RestoreGameInputFallback();
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
		if (const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this))
		{
			ChatEntryWidgetClass = WidgetDefinition->GetChatEntryWidgetClass();
		}
	}

	if (!ChatEntryWidgetClass)
	{

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

void UChatBoxWidget::ApplyWidgetDefinitionSettings()
{
	if (const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this))
	{
		const FChatWidgetSettings& Settings = WidgetDefinition->GetChatWidgetSettings();
		if (const TSubclassOf<UChatEntryWidget> ResolvedChatEntryWidgetClass =
			WidgetDefinition->GetChatEntryWidgetClass())
		{
			ChatEntryWidgetClass = ResolvedChatEntryWidgetClass;
		}
		ScrollMultiplier = FMath::Max(Settings.ScrollMultiplier, 1.0f);
		if (!Settings.ChatScrollBoxCandidateNames.IsEmpty())
		{
			ChatScrollBoxCandidateNames = Settings.ChatScrollBoxCandidateNames;
		}
		if (!Settings.ChatInputCandidateNames.IsEmpty())
		{
			ChatInputCandidateNames = Settings.ChatInputCandidateNames;
		}
	}
}

void UChatBoxWidget::CacheWidgets()
{
	UWidgetTree* CurrentWidgetTree = WidgetTree;

	CachedChatScrollBox = ScrollBox_ChatMessages;
	if (!CachedChatScrollBox)
	{
		CachedChatScrollBox = PdWidgetLookup::FindWidgetByNames<UScrollBox>(CurrentWidgetTree, ChatScrollBoxCandidateNames);
	}
	if (!CachedChatScrollBox)
	{
		CachedChatScrollBox = PdWidgetLookup::FindFirstWidgetOfType<UScrollBox>(CurrentWidgetTree);
	}

	CachedChatInputText = TxtBox_ChatInput;
	if (!CachedChatInputText)
	{
		CachedChatInputText = PdWidgetLookup::FindWidgetByNames<UEditableText>(CurrentWidgetTree, ChatInputCandidateNames);
	}
	if (!CachedChatInputText)
	{
		CachedChatInputText = PdWidgetLookup::FindFirstWidgetOfType<UEditableText>(CurrentWidgetTree);
	}

	if (CachedChatInputText)
	{
		CachedChatInputText->OnTextCommitted.RemoveDynamic(this, &ThisClass::HandleChatTextCommitted);
		CachedChatInputText->OnTextCommitted.AddUniqueDynamic(this, &ThisClass::HandleChatTextCommitted);
	}


}

bool UChatBoxWidget::ApplyRoutedChatInput(UEditableText* ChatInputText)
{
	if (!IsValid(ChatInputText))
	{
		return false;
	}

	const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	UUiSubsystem* UiSubsystem = LocalPlayer
		? LocalPlayer->GetSubsystem<UUiSubsystem>()
		: nullptr;
	if (!UiSubsystem)
	{
		return false;
	}

	FPdUiModalInputConfig InputConfig;
	InputConfig.InputMode = EPdUiInputMode::GameAndUI;
	InputConfig.bHideCursorDuringCapture = true;
	InputConfig.bShowMouseCursor = false;
	InputConfig.bEnableClickEvents = false;
	InputConfig.bEnableMouseOverEvents = false;
	InputConfig.RestorePolicy = EPdUiInputRestorePolicy::Gameplay;

	if (UiSubsystem->UpdateModalInput(
		this,
		ChatModalInputToken,
		ChatInputText,
		InputConfig))
	{
		return true;
	}

	ChatModalInputToken.Invalidate();
	ChatModalInputToken = UiSubsystem->AcquireModalInput(this, ChatInputText, InputConfig);
	return ChatModalInputToken.IsValid();
}

bool UChatBoxWidget::ReleaseRoutedChatInput()
{
	if (!ChatModalInputToken.IsValid())
	{
		return false;
	}

	bool bReleased = false;
	if (const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
	{
		if (UUiSubsystem* UiSubsystem = LocalPlayer->GetSubsystem<UUiSubsystem>())
		{
			bReleased = UiSubsystem->ReleaseModalInput(this, ChatModalInputToken);
		}
	}

	ChatModalInputToken.Invalidate();
	return bReleased;
}

void UChatBoxWidget::RestoreGameInputFallback() const
{
	APlayerController* PlayerController = GetOwningPlayer();
	if (!PlayerController)
	{
		return;
	}

	FInputModeGameOnly InputMode;
	PlayerController->SetInputMode(InputMode);
	PlayerController->bShowMouseCursor = false;
	PlayerController->bEnableClickEvents = false;
	PlayerController->bEnableMouseOverEvents = false;
}

void UChatBoxWidget::SetChatInputEnabled(const bool bEnabled) const
{
	if (UEditableText* ChatInputText = GetChatInputWidget())
	{
		ChatInputText->SetIsEnabled(bEnabled);
		ChatInputText->SetHintText(bEnabled
			? NSLOCTEXT("Chat", "ChatInputHintActive", "Enter message")
			: NSLOCTEXT("Chat", "ChatInputHintInactive", "Press Enter to chat"));
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
	return CachedChatInputText.Get();
}

UScrollBox* UChatBoxWidget::GetChatScrollBox() const
{
	return CachedChatScrollBox.Get();
}
