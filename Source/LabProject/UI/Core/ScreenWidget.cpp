#include "UI/Core/ScreenWidget.h"

#include "Components/Button.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Localization/MenuLocalizationSubsystem.h"
#include "UI/Common/MenuLocalizationExtension.h"
#include "UI/Common/UiMotion.h"
#include "UI/Core/UiSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ScreenWidget)

namespace
{
	FUIInputConfig MakeBlockingInputConfig(const ECommonInputMode InputMode, const EMouseCaptureMode MouseCapture)
	{
		FUIInputConfig Config(InputMode, MouseCapture);
		Config.bIgnoreMoveInput = Config.bIgnoreLookInput = true;
		return Config;
	}
}

void UScreenWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	IntroElapsed = 0.f;
	ApplyIntroMotion(0.f);
}

void UScreenWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!IntroElapsed.IsSet())
	{
		return;
	}
	const float Alpha = FMath::Clamp((IntroElapsed.GetValue() + InDeltaTime) / IntroDuration, 0.f, 1.f);
	ApplyIntroMotion(Alpha);
	if (Alpha >= 1.f)
	{
		IntroElapsed.Reset();
	}
	else
	{
		IntroElapsed = IntroElapsed.GetValue() + InDeltaTime;
	}
}

// 투명도는 빠르게 차오르고, 위치는 처음에 크게 움직였다가 길게 가라앉는다.
void UScreenWidget::ApplyIntroMotion(const float Alpha)
{
	SetRenderOpacity(UiMotion::EaseOutCubic(Alpha));
	SetRenderTranslation(FVector2D(0.f, IntroRise * (1.f - UiMotion::EaseOutExpo(Alpha))));
}

UScreenWidget::UScreenWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ScreenInputConfig(MakeBlockingInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture))
{
	bIsBackHandler = true;
	bAutoRestoreFocus = true;
}

void UScreenWidget::ConfigureScreen(UWidget* FocusTarget, FSimpleDelegate BackAction, const ECommonInputMode InputMode,
	const EMouseCaptureMode MouseCapture)
{
	ScreenInputConfig = MakeBlockingInputConfig(InputMode, MouseCapture);
	ScreenFocusTarget = FocusTarget;
	bAutoRestoreFocus = FocusTarget != nullptr;
	ScreenBackAction = MoveTemp(BackAction);
}

void UScreenWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	MenuLocalization = UMenuLocalizationExtension::Attach(*this, {MenuTextBindings, MenuTooltipBindings, MenuHintBindings},
		FSimpleDelegate::CreateUObject(this, &ThisClass::OnMenuLanguageChanged));
}

void UScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (Btn_GameSettings)
	{
		Btn_GameSettings->OnClicked.AddUniqueDynamic(this, &ThisClass::OpenGameSettings);
	}
}

void UScreenWidget::NativeDestruct()
{
	CloseGameSettings();
	if (Btn_GameSettings)
	{
		Btn_GameSettings->OnClicked.RemoveDynamic(this, &ThisClass::OpenGameSettings);
	}
	Super::NativeDestruct();
}

TOptional<FUIInputConfig> UScreenWidget::GetDesiredInputConfig() const
{
	return ScreenInputConfig;
}

UWidget* UScreenWidget::NativeGetDesiredFocusTarget() const
{
	return ScreenFocusTarget.Get();
}

bool UScreenWidget::NativeOnHandleBackAction()
{
	if (ScreenBackAction.IsBound())
	{
		ScreenBackAction.Execute();
	}
	else
	{
		DeactivateWidget();
	}
	return true;
}

void UScreenWidget::OpenGameSettings()
{
	ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	if (UUiSubsystem* Ui = LocalPlayer ? LocalPlayer->GetSubsystem<UUiSubsystem>() : nullptr)
	{
		Ui->OpenGameSettings(this);
	}
}

bool UScreenWidget::CloseGameSettings()
{
	ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	UUiSubsystem* Ui = LocalPlayer ? LocalPlayer->GetSubsystem<UUiSubsystem>() : nullptr;
	return Ui && Ui->CloseGameSettings(this);
}

UMenuLocalizationSubsystem* UScreenWidget::GetLocalization() const
{
	return GetGameInstance() ? GetGameInstance()->GetSubsystem<UMenuLocalizationSubsystem>() : nullptr;
}

FText UScreenWidget::MenuText(FName Key) const
{
	return UMenuLocalizationSubsystem::GetText(Key);
}

FText UScreenWidget::MenuTextOrFallback(FName Key, const FText& Fallback) const
{
	return UMenuLocalizationSubsystem::GetTextOrFallback(Key, Fallback);
}

void UScreenWidget::SetHoverTooltip(UWidget* Widget, const FText& Text)
{
	if (MenuLocalization)
	{
		MenuLocalization->SetHoverTooltip(Widget, Text);
	}
}
