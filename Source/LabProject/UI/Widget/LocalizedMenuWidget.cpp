#include "UI/Widget/LocalizedMenuWidget.h"
#include "UI/Widget/GameSettingsWidget.h"
#include "UI/UiScreen.h"
#include "Input/CommonUIActionRouterBase.h"
#include "UI/UiSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "UI/Tooltip/GameTooltipPlacement.h"
#include "Settings/MenuLocalizationSubsystem.h"
#include "Settings/MenuLocalizationSettings.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Engine/Font.h"
#include "Engine/GameInstance.h"

void ULocalizedMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UMenuLocalizationSubsystem* Localization = GetLocalization())
	{
		Localization->OnLanguageChanged.AddUniqueDynamic(this, &ThisClass::RefreshLocalizedText);
	}
	if (Btn_GameSettings) Btn_GameSettings->OnClicked.AddUniqueDynamic(this, &ThisClass::OpenGameSettings);
	ApplyLocalizedBindings();
}

void ULocalizedMenuWidget::NativeDestruct()
{
	CloseGameSettings();
	if (Btn_GameSettings) Btn_GameSettings->OnClicked.RemoveDynamic(this, &ThisClass::OpenGameSettings);
	if (UMenuLocalizationSubsystem* Localization = GetLocalization())
	{
		Localization->OnLanguageChanged.RemoveDynamic(this, &ThisClass::RefreshLocalizedText);
	}
	Super::NativeDestruct();
}

UMenuLocalizationSubsystem* ULocalizedMenuWidget::GetLocalization() const
{
	return GetGameInstance() ? GetGameInstance()->GetSubsystem<UMenuLocalizationSubsystem>() : nullptr;
}

FText ULocalizedMenuWidget::MenuText(FName Key) const
{
	return MenuTextOrFallback(Key, FText::FromName(Key));
}

FText ULocalizedMenuWidget::MenuTextOrFallback(FName Key, const FText& Fallback) const
{
	const UMenuLocalizationSubsystem* Localization = GetLocalization();
	return Localization ? Localization->GetTextOrFallback(Key, Fallback) : Fallback;
}

void ULocalizedMenuWidget::RefreshLocalizedText()
{
	ApplyLocalizedBindings();
	OnMenuLanguageChanged();
}

void ULocalizedMenuWidget::ApplyLocalizedBindings()
{
	const UMenuLocalizationSubsystem* Localization = GetLocalization();
	if (!Localization || !WidgetTree) return;
	UFont* Font = Localization->GetFontForLanguage(Localization->GetLanguage());
	WidgetTree->ForEachWidget([Font](UWidget* Widget)
	{
		if (!Font) return;
		if (UTextBlock* Text = Cast<UTextBlock>(Widget))
		{
			FSlateFontInfo Info = Text->GetFont();
			Info.FontObject = Font;
			Info.TypefaceFontName = NAME_None;
			Text->SetFont(Info);
		}
		// Keep editable fields' asset-owned style: nicknames are user content and numeric
		// inputs need no language-specific font. UE 5.8 SetWidgetStyle also retains the
		// input style address in Slate, so a temporary style here would be unsafe.
	});
	for (const auto& Binding : MenuTextBindings)
	{
		if (UTextBlock* Text = Cast<UTextBlock>(GetWidgetFromName(Binding.Key)))
			Text->SetText(Localization->GetTextOrFallback(Binding.Value, Text->GetText()));
	}
	for (const auto& Binding : MenuTooltipBindings)
	{
		if (UWidget* Widget = GetWidgetFromName(Binding.Key))
		{
			Widget->SetToolTipText(Localization->GetText(Binding.Value));
		}
	}
	for (const auto& Binding : MenuHintBindings)
	{
		if (UEditableTextBox* Edit = Cast<UEditableTextBox>(GetWidgetFromName(Binding.Key)))
			Edit->SetHintText(Localization->GetText(Binding.Value));
	}
	WidgetTree->ForEachWidget([](UWidget* Widget)
	{
		if (UButton* Button = Cast<UButton>(Widget))
		{
			GameTooltipPlacement::ApplyToButton(Button);
		}
	});
}

void ULocalizedMenuWidget::OpenGameSettings()
{
	if (ActiveGameSettings && ActiveGameSettings->GetParent() != nullptr)
	{
		if (UCommonActivatableWidget* Screen = UCommonUIActionRouterBase::FindOwningActivatable(ActiveGameSettings->GetCachedWidget(), GetOwningLocalPlayer()))
			Screen->RequestRefreshFocus();
		return;
	}
	const TSubclassOf<UGameSettingsWidget> SettingsClass = GetDefault<UMenuLocalizationSettings>()->SettingsWidgetClass.LoadSynchronous();
	if (!SettingsClass || !GetOwningPlayer()) return;
	ActiveGameSettings = CreateWidget<UGameSettingsWidget>(GetOwningPlayer(), SettingsClass);
	if (ActiveGameSettings)
	{
		UUiScreen* Screen = CreateWidget<UUiScreen>(GetOwningPlayer());
		FUIInputConfig Config(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
		Config.bIgnoreMoveInput = Config.bIgnoreLookInput = true;
		Screen->SetContent(ActiveGameSettings, Config, ActiveGameSettings->GetInitialFocusTarget(),
			FSimpleDelegate::CreateUObject(ActiveGameSettings, &UGameSettingsWidget::CloseSettings));
		GetOwningLocalPlayer()->GetSubsystem<UUiSubsystem>()->PushScreen(Screen, EUiScreenLayer::Modal);
	}
}

bool ULocalizedMenuWidget::CloseGameSettings()
{
	if (!ActiveGameSettings || ActiveGameSettings->GetParent() == nullptr) return false;
	ActiveGameSettings->CloseSettings();
	ActiveGameSettings = nullptr;
	return true;
}
