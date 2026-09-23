#include "UI/Widget/GameSettingsWidget.h"
#include "Settings/MenuLocalizationSubsystem.h"
#include "Settings/AudioSettingsSubsystem.h"
#include "UI/UiSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Font.h"
#include "InputCoreTypes.h"

void UGameSettingsWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	CB_SettingsLanguage->OnGenerateWidgetEvent.BindDynamic(this, &ThisClass::GenerateLanguageOption);
	{
		TGuardValue<bool> Guard(bSynchronizing, true);
		CB_SettingsLanguage->ClearOptions();
		for (EGuideLanguage Language : UiLanguage::All()) CB_SettingsLanguage->AddOption(UiLanguage::NativeName(Language));
	}
	CB_SettingsLanguage->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::HandleLanguageSelected);
	Btn_Done->OnClicked.AddUniqueDynamic(this, &ThisClass::CloseSettings);
	Slider_MasterVolume->OnValueChanged.AddUniqueDynamic(this, &ThisClass::HandleVolumeChanged);
	if (UAudioSettingsSubsystem* Audio = GetGameInstance()->GetSubsystem<UAudioSettingsSubsystem>())
	{
		VolumeChangedHandle = Audio->OnMasterVolumeChanged.AddUObject(this, &ThisClass::RefreshVolume);
		RefreshVolume(Audio->GetMasterVolumePercent());
	}
	OnMenuLanguageChanged();
	if (ULocalPlayer* Player = GetOwningLocalPlayer())
	{
		FUiModalInputConfig Config;
		Config.InputMode = EUiInputMode::UIOnly;
		ModalInputToken = Player->GetSubsystem<UUiSubsystem>()->AcquireModalInput(this, CB_SettingsLanguage, Config);
	}
	CB_SettingsLanguage->SetFocus();
}

void UGameSettingsWidget::NativeDestruct()
{
	CB_SettingsLanguage->OnSelectionChanged.RemoveDynamic(this, &ThisClass::HandleLanguageSelected);
	CB_SettingsLanguage->OnGenerateWidgetEvent.Unbind();
	Btn_Done->OnClicked.RemoveDynamic(this, &ThisClass::CloseSettings);
	Slider_MasterVolume->OnValueChanged.RemoveDynamic(this, &ThisClass::HandleVolumeChanged);
	if (UAudioSettingsSubsystem* Audio = GetGameInstance()->GetSubsystem<UAudioSettingsSubsystem>())
	{
		Audio->OnMasterVolumeChanged.Remove(VolumeChangedHandle);
		Audio->SaveMasterVolumeSettings();
	}
	if (ULocalPlayer* Player = GetOwningLocalPlayer())
		Player->GetSubsystem<UUiSubsystem>()->ReleaseModalInput(this, ModalInputToken);
	Super::NativeDestruct();
}

void UGameSettingsWidget::OnMenuLanguageChanged()
{
	if (!CB_SettingsLanguage || !GetLocalization()) return;
	TGuardValue<bool> Guard(bSynchronizing, true);
	CB_SettingsLanguage->SetSelectedOption(UiLanguage::NativeName(GetLocalization()->GetLanguage()));
	if (Txt_SettingsHint) Txt_SettingsHint->SetText(MenuText(bLanguageSaveFailed ? TEXT("Settings.SaveFailed") : TEXT("Settings.Hint")));
}

void UGameSettingsWidget::HandleLanguageSelected(FString Option, ESelectInfo::Type SelectionType)
{
	if (bSynchronizing || SelectionType == ESelectInfo::Direct || !GetLocalization()) return;
	for (EGuideLanguage Language : UiLanguage::All())
	{
		if (Option == UiLanguage::NativeName(Language))
		{
			bLanguageSaveFailed = !GetLocalization()->SetLanguage(Language);
			OnMenuLanguageChanged();
			break;
		}
	}
}

UWidget* UGameSettingsWidget::GenerateLanguageOption(FString Option)
{
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
	Text->SetText(FText::FromString(Option));
	FSlateFontInfo Font = Text->GetFont();
	Font.Size = 22;
	Font.TypefaceFontName = NAME_None;
	if (const UMenuLocalizationSubsystem* Localization = GetLocalization())
	{
		for (EGuideLanguage Language : UiLanguage::All())
			if (Option == UiLanguage::NativeName(Language)) Font.FontObject = Localization->GetFontForLanguage(Language);
	}
	Text->SetFont(Font);
	Text->SetColorAndOpacity(FSlateColor(FLinearColor(0.08f, 0.04f, 0.1f, 1.f)));
	Text->SetMargin(FMargin(12.f, 6.f));
	return Text;
}

void UGameSettingsWidget::HandleVolumeChanged(float Value)
{
	if (bSynchronizing) return;
	if (UAudioSettingsSubsystem* Audio = GetGameInstance()->GetSubsystem<UAudioSettingsSubsystem>())
		Audio->SetMasterVolumePercent(FMath::RoundToInt(Value * 100.f));
}

void UGameSettingsWidget::RefreshVolume(int32 Percent)
{
	TGuardValue<bool> Guard(bSynchronizing, true);
	Slider_MasterVolume->SetValue(Percent / 100.f);
	Txt_VolumeValue->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), Percent)));
}

void UGameSettingsWidget::CloseSettings() { RemoveFromParent(); }

FReply UGameSettingsWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (Event.GetKey() == EKeys::Escape) { CloseSettings(); return FReply::Handled(); }
	return Super::NativeOnKeyDown(Geometry, Event);
}
