#include "UI/Common/LocalizedMenuWidget.h"

#include "Engine/GameInstance.h"
#include "Localization/MenuLocalizationSubsystem.h"
#include "UI/Common/MenuLocalizationExtension.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LocalizedMenuWidget)

void ULocalizedMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	MenuLocalization = UMenuLocalizationExtension::Attach(*this, {MenuTextBindings, MenuTooltipBindings, MenuHintBindings},
		FSimpleDelegate::CreateUObject(this, &ThisClass::OnMenuLanguageChanged));
}

UMenuLocalizationSubsystem* ULocalizedMenuWidget::GetLocalization() const
{
	return GetGameInstance() ? GetGameInstance()->GetSubsystem<UMenuLocalizationSubsystem>() : nullptr;
}

FText ULocalizedMenuWidget::MenuText(FName Key) const
{
	return UMenuLocalizationSubsystem::GetText(Key);
}

FText ULocalizedMenuWidget::MenuTextOrFallback(FName Key, const FText& Fallback) const
{
	return UMenuLocalizationSubsystem::GetTextOrFallback(Key, Fallback);
}

void ULocalizedMenuWidget::SetHoverTooltip(UWidget* Widget, const FText& Text)
{
	if (MenuLocalization)
	{
		MenuLocalization->SetHoverTooltip(Widget, Text);
	}
}
