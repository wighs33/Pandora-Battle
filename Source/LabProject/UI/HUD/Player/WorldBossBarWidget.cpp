#include "UI/HUD/Player/WorldBossBarWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Localization/MenuLocalizationSubsystem.h"
#include "Mode/WorldBossRaid.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WorldBossBarWidget)

namespace
{
	// MenuText string table keys
	const FName BossNameKey(TEXT("HUD.WorldBossName"));
	const FName AttemptKey(TEXT("HUD.WorldBossAttempt"));
	const FName AttemptEndedKey(TEXT("HUD.WorldBossAttemptEnded"));
	const FName DefeatedKey(TEXT("HUD.WorldBossDefeated"));

	// 1920 x 1080 기준. 화면 가운데 위쪽, 다른 HUD와 겹치지 않는 폭이다.
	constexpr float BarWidth = 900.0f;
	constexpr float BarHeight = 22.0f;
	constexpr float BarFrameThickness = 2.0f;
	/** 잔상 막대가 실제 체력을 따라가는 빠르기(초당 남은 차이의 비율). */
	constexpr float TrailCatchUpSpeed = 3.0f;

	const FLinearColor FrameColor(0.0f, 0.0f, 0.0f, 0.85f);
	const FLinearColor BarBackgroundColor(0.06f, 0.02f, 0.02f, 0.9f);
	const FLinearColor HealthColor(0.72f, 0.06f, 0.04f, 1.0f);
	const FLinearColor TrailColor(1.0f, 0.78f, 0.45f, 1.0f);

	enum class EAttemptPhase : int32
	{
		None,
		Running,
		Ended,
		BossDefeated
	};
}

void UWorldBossBarWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>();
	Root->SetWidthOverride(BarWidth);
	WidgetTree->RootWidget = Root;

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Root->AddChild(Column);

	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>();
	Column->AddChildToVerticalBox(Header);
	NameText = MakeText(20, ETextJustify::Left);
	Header->AddChildToHorizontalBox(NameText)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	HealthText = MakeText(16, ETextJustify::Right);
	Header->AddChildToHorizontalBox(HealthText)->SetVerticalAlignment(VAlign_Bottom);

	// 테두리 안에 잔상 막대를 깔고, 그 위에 바탕이 투명한 체력 막대를 겹친다.
	UBorder* Frame = WidgetTree->ConstructWidget<UBorder>();
	Frame->SetBrush(FSlateColorBrush(FrameColor));
	Frame->SetPadding(FMargin(BarFrameThickness));
	Column->AddChildToVerticalBox(Frame)->SetPadding(FMargin(0.0f, 4.0f));
	USizeBox* BarSize = WidgetTree->ConstructWidget<USizeBox>();
	BarSize->SetHeightOverride(BarHeight);
	Frame->SetContent(BarSize);
	UOverlay* Bars = WidgetTree->ConstructWidget<UOverlay>();
	BarSize->AddChild(Bars);
	TrailBar = MakeBar(BarBackgroundColor, TrailColor);
	HealthBar = MakeBar(FLinearColor::Transparent, HealthColor);
	for (UProgressBar* Bar : {TrailBar.Get(), HealthBar.Get()})
	{
		UOverlaySlot* BarSlot = Bars->AddChildToOverlay(Bar);
		BarSlot->SetHorizontalAlignment(HAlign_Fill);
		BarSlot->SetVerticalAlignment(VAlign_Fill);
	}

	AttemptText = MakeText(16, ETextJustify::Center);
	Column->AddChildToVerticalBox(AttemptText)->SetHorizontalAlignment(HAlign_Center);

	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UWorldBossBarWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UMenuLocalizationSubsystem* Localization = UGameInstance::GetSubsystem<UMenuLocalizationSubsystem>(GetGameInstance()))
	{
		Localization->OnLanguageChanged.AddUniqueDynamic(this, &ThisClass::HandleLanguageChanged);
	}
	HandleLanguageChanged();
}

void UWorldBossBarWidget::NativeDestruct()
{
	if (UMenuLocalizationSubsystem* Localization = UGameInstance::GetSubsystem<UMenuLocalizationSubsystem>(GetGameInstance()))
	{
		Localization->OnLanguageChanged.RemoveDynamic(this, &ThisClass::HandleLanguageChanged);
	}
	Super::NativeDestruct();
}

void UWorldBossBarWidget::HandleLanguageChanged()
{
	ApplyLanguageFonts();
	NameText->SetText(UMenuLocalizationSubsystem::GetText(BossNameKey));
	Shown = FShownValues();
	RefreshTexts();
}

void UWorldBossBarWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const AWorldBossRaid* CurrentRaid = Raid.Get();
	const float MaxHealth = CurrentRaid ? CurrentRaid->GetBossMaxHealth() : 0.0f;
	const float HealthPercent = MaxHealth > 0.0f ? FMath::Clamp(CurrentRaid->GetBossHealth() / MaxHealth, 0.0f, 1.0f) : 1.0f;
	// 체력이 오르면(새 보스, 다시 맞춘 체력) 잔상 없이 바로 맞춘다.
	TrailPercent = HealthPercent > TrailPercent
		? HealthPercent
		: FMath::FInterpTo(TrailPercent, HealthPercent, InDeltaTime, TrailCatchUpSpeed);
	HealthBar->SetPercent(HealthPercent);
	TrailBar->SetPercent(TrailPercent);
	RefreshTexts();
}

void UWorldBossBarWidget::RefreshTexts()
{
	const AWorldBossRaid* CurrentRaid = Raid.Get();
	const APlayerController* Player = GetOwningPlayer();
	const FWorldBossAttempt* Attempt = CurrentRaid && Player ? CurrentRaid->FindAttempt(Player->PlayerState) : nullptr;

	FShownValues Values;
	Values.Health = CurrentRaid ? FMath::CeilToInt64(CurrentRaid->GetBossHealth()) : 0;
	Values.MaxHealth = CurrentRaid ? FMath::RoundToInt64(CurrentRaid->GetBossMaxHealth()) : 0;
	Values.SecondsLeft = Attempt ? FMath::CeilToInt64(CurrentRaid->GetAttemptSecondsLeft(*Attempt)) : 0;
	Values.Damage = Attempt ? FMath::FloorToInt64(Attempt->Damage) : 0;
	const EAttemptPhase Phase = CurrentRaid && CurrentRaid->IsBossDefeated() ? EAttemptPhase::BossDefeated
		: !Attempt ? EAttemptPhase::None
		: Attempt->bEnded ? EAttemptPhase::Ended
		: EAttemptPhase::Running;
	Values.Phase = static_cast<int32>(Phase);
	if (Values == Shown)
	{
		return;
	}
	Shown = Values;

	const double Percent = Values.MaxHealth > 0 ? 100.0 * Values.Health / Values.MaxHealth : 0.0;
	FNumberFormattingOptions PercentFormat;
	PercentFormat.MinimumFractionalDigits = 2;
	PercentFormat.MaximumFractionalDigits = 2;
	HealthText->SetText(FText::Format(INVTEXT("{0} / {1}  ({2}%)"),
		FText::AsNumber(Values.Health), FText::AsNumber(Values.MaxHealth), FText::AsNumber(Percent, &PercentFormat)));

	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("Time"), FText::AsCultureInvariant(FString::Printf(TEXT("%02lld:%02lld"), Values.SecondsLeft / 60, Values.SecondsLeft % 60)));
	Arguments.Add(TEXT("Damage"), FText::AsNumber(Values.Damage));
	switch (Phase)
	{
	case EAttemptPhase::Running:
		AttemptText->SetText(FText::Format(UMenuLocalizationSubsystem::GetText(AttemptKey), Arguments));
		break;
	case EAttemptPhase::Ended:
		AttemptText->SetText(FText::Format(UMenuLocalizationSubsystem::GetText(AttemptEndedKey), Arguments));
		break;
	case EAttemptPhase::BossDefeated:
		AttemptText->SetText(UMenuLocalizationSubsystem::GetText(DefeatedKey));
		break;
	case EAttemptPhase::None:
		AttemptText->SetText(FText::GetEmpty());
		break;
	}
}

UTextBlock* UWorldBossBarWidget::MakeText(const int32 FontSize, const ETextJustify::Type Justification)
{
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
	FSlateFontInfo Font = Text->GetFont();
	Font.Size = FontSize;
	Font.OutlineSettings.OutlineSize = 1;
	Font.OutlineSettings.OutlineColor = FLinearColor::Black;
	Text->SetFont(Font);
	Text->SetJustification(Justification);
	Text->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	return Text;
}

UProgressBar* UWorldBossBarWidget::MakeBar(const FLinearColor& BackgroundColor, const FLinearColor& FillColor)
{
	UProgressBar* Bar = WidgetTree->ConstructWidget<UProgressBar>();
	FProgressBarStyle Style = Bar->GetWidgetStyle();
	Style.BackgroundImage = FSlateColorBrush(BackgroundColor);
	// 채움 색은 SetFillColorAndOpacity로 정하므로 흰 바탕을 쓴다.
	Style.FillImage = FSlateColorBrush(FLinearColor::White);
	Bar->SetWidgetStyle(Style);
	Bar->SetFillColorAndOpacity(FillColor);
	Bar->SetPercent(1.0f);
	return Bar;
}

void UWorldBossBarWidget::ApplyLanguageFonts()
{
	const UMenuLocalizationSubsystem* Localization = UGameInstance::GetSubsystem<UMenuLocalizationSubsystem>(GetGameInstance());
	if (!Localization)
	{
		return;
	}
	for (UTextBlock* Text : {NameText.Get(), HealthText.Get(), AttemptText.Get()})
	{
		Localization->ApplyLanguageFont(*Text);
	}
}

