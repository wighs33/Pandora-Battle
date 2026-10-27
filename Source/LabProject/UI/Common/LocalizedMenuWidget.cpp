#include "UI/Common/LocalizedMenuWidget.h"
#include "UI/Core/UiSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "UI/Tooltip/MenuHoverTooltip.h"
#include "Localization/MenuLocalizationSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"

namespace
{
	/** 설명 패널은 어느 메뉴 화면보다도 위에 그린다. */
	constexpr int32 HoverTooltipZOrder = 10000;
}

void ULocalizedMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UMenuLocalizationSubsystem* Localization = GetLocalization())
	{
		Localization->OnLanguageChanged.AddUniqueDynamic(this, &ThisClass::RefreshLocalizedText);
	}
	if (Btn_GameSettings)
	{
		Btn_GameSettings->OnClicked.AddUniqueDynamic(this, &ThisClass::OpenGameSettings);
	}
	ApplyLocalizedBindings();
	// 위젯 Tick은 Slate가 그리는 도중에 돌아 커서 아래 위젯을 아직 다 모르므로, 그리기 전(PreTick)에 지난 프레임 화면으로 찾는다.
	if (FSlateApplication::IsInitialized())
	{
		HoverTooltipUpdateHandle = FSlateApplication::Get().OnPreTick().AddWeakLambda(this, [this](float) { UpdateHoverTooltip(); });
	}
}

void ULocalizedMenuWidget::NativeDestruct()
{
	CloseGameSettings();
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().OnPreTick().Remove(HoverTooltipUpdateHandle);
	}
	HoverTooltipUpdateHandle.Reset();
	if (HoverTooltip)
	{
		HoverTooltip->RemoveFromParent();
		HoverTooltip = nullptr;
	}
	if (Btn_GameSettings)
	{
		Btn_GameSettings->OnClicked.RemoveDynamic(this, &ThisClass::OpenGameSettings);
	}
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
	if (!Localization || !WidgetTree)
	{
		return;
	}
	WidgetTree->ForEachWidget([Localization](UWidget* Widget)
	{
		if (UTextBlock* Text = Cast<UTextBlock>(Widget))
		{
			Localization->ApplyLanguageFont(*Text);
		}
		// 입력 칸은 애셋에 지정한 스타일을 그대로 둔다. 닉네임은 사용자가 쓴 내용이고 숫자 입력은 언어별 글꼴이 필요 없다.
		// UE 5.8의 SetWidgetStyle은 입력 스타일의 주소를 Slate에 넘겨 계속 쓰므로, 여기서 임시 스타일을 만들면 안전하지 않다.
	});
	for (const auto& Binding : MenuTextBindings)
	{
		if (UTextBlock* Text = Cast<UTextBlock>(GetWidgetFromName(Binding.Key)))
		{
			Text->SetText(Localization->GetTextOrFallback(Binding.Value, Text->GetText()));
		}
	}
	for (const auto& Binding : MenuTooltipBindings)
	{
		SetHoverTooltip(GetWidgetFromName(Binding.Key), Localization->GetText(Binding.Value));
	}
	for (const auto& Binding : MenuHintBindings)
	{
		if (UEditableTextBox* Edit = Cast<UEditableTextBox>(GetWidgetFromName(Binding.Key)))
		{
			Edit->SetHintText(Localization->GetText(Binding.Value));
		}
	}
	// 연결 없이 에셋에 직접 적은 툴팁도 같은 설명 패널로 옮긴다. 옮긴 뒤에는 엔진 툴팁이 비어 다시 옮겨지지 않는다.
	WidgetTree->ForEachWidget([this](UWidget* Widget)
	{
		if (!Widget->GetToolTipText().IsEmpty())
		{
			SetHoverTooltip(Widget, Widget->GetToolTipText());
		}
	});
}

void ULocalizedMenuWidget::SetHoverTooltip(UWidget* Widget, const FText& Text)
{
	if (Widget)
	{
		Widget->SetToolTipText(FText::GetEmpty());
		HoverTooltipTexts.Add(Widget, Text);
	}
}

void ULocalizedMenuWidget::UpdateHoverTooltip()
{
	UWidget* Widget = FindHoveredTooltipWidget();
	const FText* Text = Widget ? HoverTooltipTexts.Find(Widget) : nullptr;
	if (!Text || Text->IsEmptyOrWhitespace())
	{
		if (HoverTooltip)
		{
			HoverTooltip->Hide();
		}
		return;
	}
	if (!HoverTooltip)
	{
		HoverTooltip = CreateWidget<UMenuHoverTooltip>(this, UMenuHoverTooltip::StaticClass());
		HoverTooltip->AddToViewport(HoverTooltipZOrder);
	}
	const UMenuLocalizationSubsystem* Localization = GetLocalization();
	HoverTooltip->ShowFor(*Widget, *Text, Localization ? Localization->GetCurrentFont() : nullptr);
}

// Slate의 마우스 진입 이벤트는 비활성 위젯을 건너뛰므로, 닫힌 버튼까지 찾도록 활성 여부를 무시하고 커서 아래 위젯 경로를 직접 구한다.
// 다른 화면이 위를 덮고 있거나 이 화면이 접혀 있으면 이 화면의 위젯이 경로에 없으므로 설명이 뜨지 않는다.
UWidget* ULocalizedMenuWidget::FindHoveredTooltipWidget() const
{
	const APlayerController* Player = GetOwningPlayer();
	if (HoverTooltipTexts.IsEmpty() || !Player || !Player->ShouldShowMouseCursor() || !FSlateApplication::IsInitialized())
	{
		return nullptr;
	}
	FSlateApplication& Slate = FSlateApplication::Get();
	const FWidgetPath Path = Slate.LocateWindowUnderMouse(Slate.GetCursorPos(), Slate.GetInteractiveTopLevelWindows(), true);
	for (int32 Index = Path.Widgets.Num() - 1; Index >= 0; --Index)
	{
		const SWidget* PathWidget = &Path.Widgets[Index].Widget.Get();
		for (const TPair<TWeakObjectPtr<UWidget>, FText>& Entry : HoverTooltipTexts)
		{
			if (Entry.Key.IsValid() && Entry.Key->GetCachedWidget().Get() == PathWidget)
			{
				return Entry.Key.Get();
			}
		}
	}
	return nullptr;
}

void ULocalizedMenuWidget::OpenGameSettings()
{
	ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	if (UUiSubsystem* Ui = LocalPlayer ? LocalPlayer->GetSubsystem<UUiSubsystem>() : nullptr)
	{
		Ui->OpenGameSettings(this);
	}
}

bool ULocalizedMenuWidget::CloseGameSettings()
{
	ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	UUiSubsystem* Ui = LocalPlayer ? LocalPlayer->GetSubsystem<UUiSubsystem>() : nullptr;
	return Ui && Ui->CloseGameSettings(this);
}
