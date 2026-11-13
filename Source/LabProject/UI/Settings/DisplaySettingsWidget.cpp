#include "UI/Settings/DisplaySettingsWidget.h"

#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Localization/MenuLocalizationSubsystem.h"
#include "Settings/DisplaySettingsSubsystem.h"
#include "UI/Settings/SettingsComboOption.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DisplaySettingsWidget)

namespace
{
	FName WindowModeTextKey(const EWindowMode::Type WindowMode)
	{
		switch (WindowMode)
		{
		case EWindowMode::Fullscreen:
			return TEXT("Settings.WindowMode.Fullscreen");
		case EWindowMode::WindowedFullscreen:
			return TEXT("Settings.WindowMode.Borderless");
		default:
			return TEXT("Settings.WindowMode.Windowed");
		}
	}

	/** 그래픽 품질 0~3의 이름. */
	const FName QualityTextKeys[UDisplaySettingsSubsystem::QualityLevelCount] = {
		TEXT("Settings.Quality.Low"), TEXT("Settings.Quality.Medium"), TEXT("Settings.Quality.High"), TEXT("Settings.Quality.Highest")};

	FString ResolutionText(const FIntPoint& Resolution)
	{
		return FString::Printf(TEXT("%d × %d"), Resolution.X, Resolution.Y);
	}

	/** 목록을 새로 채우고 줄 하나를 고른다. 고를 줄이 없으면(INDEX_NONE) 비워 둔다. */
	void FillCombo(UComboBoxString& Combo, const TArray<FString>& Options, const int32 SelectedIndex)
	{
		Combo.ClearOptions();
		for (const FString& Option : Options)
		{
			Combo.AddOption(Option);
		}
		if (Options.IsValidIndex(SelectedIndex))
		{
			Combo.SetSelectedIndex(SelectedIndex);
		}
		else
		{
			Combo.ClearSelection();
		}
	}
}

void UDisplaySettingsWidget::NativeConstruct()
{
	Super::NativeConstruct();
	for (UComboBoxString* Combo : {CB_WindowMode.Get(), CB_Resolution.Get(), CB_GraphicsQuality.Get(), CB_FrameRateLimit.Get()})
	{
		Combo->OnGenerateWidgetEvent.BindDynamic(this, &ThisClass::GenerateOption);
	}
	CB_WindowMode->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::HandleWindowModeSelected);
	CB_Resolution->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::HandleResolutionSelected);
	CB_GraphicsQuality->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::HandleQualitySelected);
	CB_FrameRateLimit->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::HandleFrameRateLimitSelected);
	RefreshOptions();
}

void UDisplaySettingsWidget::NativeDestruct()
{
	CB_WindowMode->OnSelectionChanged.RemoveDynamic(this, &ThisClass::HandleWindowModeSelected);
	CB_Resolution->OnSelectionChanged.RemoveDynamic(this, &ThisClass::HandleResolutionSelected);
	CB_GraphicsQuality->OnSelectionChanged.RemoveDynamic(this, &ThisClass::HandleQualitySelected);
	CB_FrameRateLimit->OnSelectionChanged.RemoveDynamic(this, &ThisClass::HandleFrameRateLimitSelected);
	for (UComboBoxString* Combo : {CB_WindowMode.Get(), CB_Resolution.Get(), CB_GraphicsQuality.Get(), CB_FrameRateLimit.Get()})
	{
		Combo->OnGenerateWidgetEvent.Unbind();
	}
	Super::NativeDestruct();
}

void UDisplaySettingsWidget::OnMenuLanguageChanged()
{
	RefreshOptions();
}

void UDisplaySettingsWidget::HandleWindowModeSelected(FString Option, ESelectInfo::Type SelectionType)
{
	const TConstArrayView<EWindowMode::Type> WindowModes = UDisplaySettingsSubsystem::GetWindowModes();
	const int32 Index = CB_WindowMode->GetSelectedIndex();
	UDisplaySettingsSubsystem* Settings = GetDisplaySettings();
	if (IsUserSelection(SelectionType) && Settings && WindowModes.IsValidIndex(Index))
	{
		Settings->SetWindowMode(WindowModes[Index]);
		RefreshResolutionOptions();
	}
}

void UDisplaySettingsWidget::HandleResolutionSelected(FString Option, ESelectInfo::Type SelectionType)
{
	const int32 Index = CB_Resolution->GetSelectedIndex();
	UDisplaySettingsSubsystem* Settings = GetDisplaySettings();
	if (IsUserSelection(SelectionType) && Settings && ResolutionOptions.IsValidIndex(Index))
	{
		Settings->SetResolution(ResolutionOptions[Index]);
	}
}

void UDisplaySettingsWidget::HandleQualitySelected(FString Option, ESelectInfo::Type SelectionType)
{
	const int32 Index = CB_GraphicsQuality->GetSelectedIndex();
	UDisplaySettingsSubsystem* Settings = GetDisplaySettings();
	if (IsUserSelection(SelectionType) && Settings && Index != INDEX_NONE)
	{
		Settings->SetQualityLevel(Index);
	}
}

void UDisplaySettingsWidget::HandleFrameRateLimitSelected(FString Option, ESelectInfo::Type SelectionType)
{
	const TConstArrayView<int32> Limits = UDisplaySettingsSubsystem::GetFrameRateLimits();
	const int32 Index = CB_FrameRateLimit->GetSelectedIndex();
	UDisplaySettingsSubsystem* Settings = GetDisplaySettings();
	if (IsUserSelection(SelectionType) && Settings && Limits.IsValidIndex(Index))
	{
		Settings->SetFrameRateLimit(Limits[Index]);
	}
}

UWidget* UDisplaySettingsWidget::GenerateOption(FString Option)
{
	const UMenuLocalizationSubsystem* Localization = GetLocalization();
	return SettingsComboOption::Make(*WidgetTree, Option, Localization ? Localization->GetCurrentFont() : nullptr);
}

void UDisplaySettingsWidget::RefreshOptions()
{
	const UDisplaySettingsSubsystem* Settings = GetDisplaySettings();
	if (!Settings)
	{
		return;
	}
	TGuardValue<bool> Guard(bSynchronizing, true);

	TArray<FString> WindowModeNames;
	for (const EWindowMode::Type WindowMode : UDisplaySettingsSubsystem::GetWindowModes())
	{
		WindowModeNames.Add(MenuText(WindowModeTextKey(WindowMode)).ToString());
	}
	FillCombo(*CB_WindowMode, WindowModeNames, UDisplaySettingsSubsystem::GetWindowModes().Find(Settings->GetWindowMode()));

	TArray<FString> QualityNames;
	for (const FName Key : QualityTextKeys)
	{
		QualityNames.Add(MenuText(Key).ToString());
	}
	FillCombo(*CB_GraphicsQuality, QualityNames, Settings->GetQualityLevel());

	TArray<FString> LimitNames;
	for (const int32 Limit : UDisplaySettingsSubsystem::GetFrameRateLimits())
	{
		LimitNames.Add(Limit > 0 ? FString::Printf(TEXT("%d FPS"), Limit) : MenuText(TEXT("Settings.FrameRate.Unlimited")).ToString());
	}
	FillCombo(*CB_FrameRateLimit, LimitNames, UDisplaySettingsSubsystem::GetFrameRateLimits().Find(Settings->GetFrameRateLimit()));

	RefreshResolutionOptions();
}

void UDisplaySettingsWidget::RefreshResolutionOptions()
{
	const UDisplaySettingsSubsystem* Settings = GetDisplaySettings();
	if (!Settings)
	{
		return;
	}
	TGuardValue<bool> Guard(bSynchronizing, true);
	ResolutionOptions = Settings->GetResolutionOptions();
	TArray<FString> Names;
	for (const FIntPoint& Resolution : ResolutionOptions)
	{
		Names.Add(ResolutionText(Resolution));
	}
	FillCombo(*CB_Resolution, Names, ResolutionOptions.Find(Settings->GetResolution()));
	// 창 전체 화면은 바탕화면 크기로만 그리므로 바꿀 수 없다.
	CB_Resolution->SetIsEnabled(Settings->CanChooseResolution());
}

bool UDisplaySettingsWidget::IsUserSelection(const ESelectInfo::Type SelectionType) const
{
	return !bSynchronizing && SelectionType != ESelectInfo::Direct;
}

UDisplaySettingsSubsystem* UDisplaySettingsWidget::GetDisplaySettings() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UDisplaySettingsSubsystem>() : nullptr;
}
