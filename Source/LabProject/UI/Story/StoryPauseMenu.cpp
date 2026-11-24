#include "UI/Story/StoryPauseMenu.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateNoResource.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Localization/MenuLocalizationSubsystem.h"
#include "Story/StoryDirector.h"
#include "UI/Common/ButtonClickRelay.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StoryPauseMenu)

namespace
{
	constexpr int32 PauseRowTextSize = 34;
	const FMargin PauseRowPadding(0.f, 10.f);
	const FLinearColor PauseRowColor(0.94f, 0.91f, 0.84f, 0.8f);
	const FLinearColor PauseRowHoverColor(1.f, 0.82f, 0.42f, 1.f);
	const FLinearColor PauseBackdropColor(0.01f, 0.008f, 0.02f, 0.72f);
}

UStoryPauseMenu::UStoryPauseMenu(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
	IntroRise = 0.f;
}

UStoryPauseMenu* UStoryPauseMenu::Create(APlayerController* OwningPlayer, FSimpleDelegate InOnClosed)
{
	UStoryPauseMenu* Menu = CreateWidget<UStoryPauseMenu>(OwningPlayer);
	if (Menu)
	{
		Menu->OnClosed = MoveTemp(InOnClosed);
	}
	return Menu;
}

void UStoryPauseMenu::NativeOnInitialized()
{
	UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>();
	Backdrop->SetBrush(FSlateColorBrush(PauseBackdropColor));
	Backdrop->SetHorizontalAlignment(HAlign_Center);
	Backdrop->SetVerticalAlignment(VAlign_Center);
	WidgetTree->RootWidget = Backdrop;
	UVerticalBox* List = WidgetTree->ConstructWidget<UVerticalBox>();
	Backdrop->SetContent(List);

	ResumeButton = AddRow(*List, TEXT("Story.Pause.Resume"), FSimpleDelegate::CreateUObject(this, &ThisClass::HandleResume));
	AddRow(*List, TEXT("Story.Pause.Skip"), FSimpleDelegate::CreateUObject(this, &ThisClass::HandleSkip));
	AddRow(*List, TEXT("Story.Pause.Settings"), FSimpleDelegate::CreateUObject(this, &ThisClass::HandleSettings));
	AddRow(*List, TEXT("Story.Pause.Title"), FSimpleDelegate::CreateUObject(this, &ThisClass::HandleTitle));

	Super::NativeOnInitialized();
}

UButton* UStoryPauseMenu::AddRow(UVerticalBox& List, const FName TextKey, FSimpleDelegate OnClicked)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>();
	FButtonStyle Style;
	Style.SetNormal(FSlateNoResource()).SetHovered(FSlateNoResource()).SetPressed(FSlateNoResource()).SetDisabled(FSlateNoResource());
	Button->SetStyle(Style);

	UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
	FSlateFontInfo Font = Label->GetFont();
	Font.Size = PauseRowTextSize;
	Label->SetFont(Font);
	Label->SetColorAndOpacity(PauseRowColor);
	Label->SetText(UMenuLocalizationSubsystem::GetStoryText(TextKey));
	if (const UMenuLocalizationSubsystem* Localization = GetLocalization())
	{
		Localization->ApplyLanguageFont(*Label);
	}
	Button->AddChild(Label);
	UVerticalBoxSlot* RowSlot = List.AddChildToVerticalBox(Button);
	RowSlot->SetHorizontalAlignment(HAlign_Center);
	RowSlot->SetPadding(PauseRowPadding);

	UButtonClickRelay* Relay = NewObject<UButtonClickRelay>(this);
	Relay->Bind(Button, MoveTemp(OnClicked), UButtonClickRelay::FOnHoverChanged::CreateWeakLambda(Label, [Label](const bool bHovered)
	{
		Label->SetColorAndOpacity(bHovered ? PauseRowHoverColor : PauseRowColor);
	}));
	Relays.Add(Relay);
	return Button;
}

void UStoryPauseMenu::NativeDestruct()
{
	for (UButtonClickRelay* Relay : Relays)
	{
		Relay->Unbind();
	}
	Relays.Reset();
	Super::NativeDestruct();
}

UWidget* UStoryPauseMenu::NativeGetDesiredFocusTarget() const
{
	return ResumeButton;
}

bool UStoryPauseMenu::NativeOnHandleBackAction()
{
	CloseMenu();
	return true;
}

void UStoryPauseMenu::CloseMenu()
{
	DeactivateWidget();
	OnClosed.ExecuteIfBound();
}

void UStoryPauseMenu::HandleResume()
{
	CloseMenu();
}

void UStoryPauseMenu::HandleSkip()
{
	CloseMenu();
	if (AStoryDirector* Director = AStoryDirector::Find(this))
	{
		Director->SkipChapter();
	}
}

void UStoryPauseMenu::HandleSettings()
{
	OpenGameSettings();
}

void UStoryPauseMenu::HandleTitle()
{
	CloseMenu();
	if (AStoryDirector* Director = AStoryDirector::Find(this))
	{
		Director->ReturnToTitle();
	}
}
