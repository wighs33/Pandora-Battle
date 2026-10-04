#include "UI/Settings/GameSettingsWidget.h"
#include "Input/CommonUIActionRouterBase.h"
#include "CommonActivatableWidget.h"
#include "Localization/MenuLocalizationSubsystem.h"
#include "Audio/AudioSettingsSubsystem.h"
#include "UI/Core/UiSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Definition/Level/LevelDefinition.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Font.h"
#include "InputCoreTypes.h"
#include "Settings/LocalPlayerSettingsSubsystem.h"
#include "UI/Core/UiScreen.h"
#include "UI/Core/WidgetClassDefinition.h"
#include "UI/Guide/GuideWidget.h"
#include "UI/Settings/AudioVolumeControl.h"

namespace
{
	constexpr int32 GeneralPageIndex = 0;
	constexpr int32 TrainingRoomPageIndex = 1;
}

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
	SoundButtonControl = NewObject<UAudioVolumeControl>(this);
	SoundButtonControl->Initialize(this, nullptr, Btn_Sound);
	if (const ULocalPlayerSettingsSubsystem* Settings = ULocalPlayerSettingsSubsystem::Get(GetOwningPlayer()))
	{
		TGuardValue<bool> Guard(bSynchronizing, true);
		Slider_MouseSensitivity->SetValue(Settings->GetMouseSensitivitySliderValue());
	}
	Slider_MouseSensitivity->OnValueChanged.AddUniqueDynamic(this, &ThisClass::HandleMouseSensitivityChanged);
	RefreshMouseSensitivityText();
	Btn_Guide->OnClicked.AddUniqueDynamic(this, &ThisClass::OpenGuide);
	Btn_TabGeneral->OnClicked.AddUniqueDynamic(this, &ThisClass::ShowGeneralPage);
	Btn_TabTrainingRoom->OnClicked.AddUniqueDynamic(this, &ThisClass::ShowTrainingRoomPage);
	SettingsTabs->SetVisibility(ULevelDefinition::IsTrainingRoomWorld(this)
		? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	ShowPage(GeneralPageIndex);
	OnMenuLanguageChanged();
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
	if (SoundButtonControl)
	{
		SoundButtonControl->Shutdown();
		SoundButtonControl = nullptr;
	}
	Slider_MouseSensitivity->OnValueChanged.RemoveDynamic(this, &ThisClass::HandleMouseSensitivityChanged);
	if (ULocalPlayerSettingsSubsystem* Settings = ULocalPlayerSettingsSubsystem::Get(GetOwningPlayer()))
	{
		Settings->SaveInputSettings();
	}
	Btn_Guide->OnClicked.RemoveDynamic(this, &ThisClass::OpenGuide);
	Btn_TabGeneral->OnClicked.RemoveDynamic(this, &ThisClass::ShowGeneralPage);
	Btn_TabTrainingRoom->OnClicked.RemoveDynamic(this, &ThisClass::ShowTrainingRoomPage);
	CloseGuide();
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

void UGameSettingsWidget::HandleMouseSensitivityChanged(float Value)
{
	if (bSynchronizing) return;
	if (ULocalPlayerSettingsSubsystem* Settings = ULocalPlayerSettingsSubsystem::Get(GetOwningPlayer()))
		Settings->SetMouseSensitivitySliderValue(Value);
	RefreshMouseSensitivityText();
}

void UGameSettingsWidget::RefreshMouseSensitivityText()
{
	if (const ULocalPlayerSettingsSubsystem* Settings = ULocalPlayerSettingsSubsystem::Get(GetOwningPlayer()))
		Txt_MouseSensitivityValue->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), Settings->GetMouseSensitivityPercent())));
}

// 가이드는 설정 화면 위 같은 층에 올린다. 설정이 바로 아래에 있으므로 가이드 안의 설정 버튼은 숨긴다.
void UGameSettingsWidget::OpenGuide()
{
	APlayerController* Controller = GetOwningPlayer();
	const UWidgetClassDefinition* Definition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this);
	const TSubclassOf<UGuideWidget> GuideClass = Definition ? Definition->GetGuideWidgetClass() : nullptr;
	if (!Controller || !GuideClass || ActiveGuideWidget) return;
	ActiveGuideWidget = CreateWidget<UGuideWidget>(Controller, GuideClass);
	if (!ActiveGuideWidget) return;
	ActiveGuideWidget->SetOpenedFromGameSettings(true);
	ActiveGuideWidget->OnGuideClosed.AddUniqueDynamic(this, &ThisClass::HandleGuideClosed);
	GetOwningLocalPlayer()->GetSubsystem<UUiSubsystem>()->PushScreen(UUiScreen::CreateBlocking(Controller, ActiveGuideWidget,
		ActiveGuideWidget, FSimpleDelegate::CreateUObject(ActiveGuideWidget, &UGuideWidget::CloseGuide)), EUiScreenLayer::Modal);
	ActiveGuideWidget->RefreshGuide();
}

void UGameSettingsWidget::HandleGuideClosed(UGuideWidget* ClosedGuideWidget)
{
	if (ClosedGuideWidget != ActiveGuideWidget) return;
	ActiveGuideWidget->OnGuideClosed.RemoveDynamic(this, &ThisClass::HandleGuideClosed);
	ActiveGuideWidget = nullptr;
}

void UGameSettingsWidget::CloseGuide()
{
	if (UGuideWidget* Guide = ActiveGuideWidget)
	{
		Guide->OnGuideClosed.RemoveDynamic(this, &ThisClass::HandleGuideClosed);
		ActiveGuideWidget = nullptr;
		Guide->CloseGuide();
	}
}

void UGameSettingsWidget::ShowGeneralPage()
{
	ShowPage(GeneralPageIndex);
}

void UGameSettingsWidget::ShowTrainingRoomPage()
{
	ShowPage(TrainingRoomPageIndex);
}

void UGameSettingsWidget::ShowPage(int32 PageIndex)
{
	Switcher_SettingsPages->SetActiveWidgetIndex(PageIndex);
	Btn_TabGeneral->SetBackgroundColor(PageIndex == GeneralPageIndex ? SelectedTabColor : UnselectedTabColor);
	Btn_TabTrainingRoom->SetBackgroundColor(PageIndex == TrainingRoomPageIndex ? SelectedTabColor : UnselectedTabColor);
}

void UGameSettingsWidget::CloseSettings()
{
    if (UCommonActivatableWidget* Screen = UCommonUIActionRouterBase::FindOwningActivatable(GetCachedWidget(), GetOwningLocalPlayer()))
        Screen->DeactivateWidget();
    RemoveFromParent();
}
