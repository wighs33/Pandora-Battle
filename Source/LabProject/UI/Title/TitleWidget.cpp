#include "UI/Title/TitleWidget.h"

#include "UI/Common/ButtonClickBinding.h"
#include "UI/Common/EditorTransactionReset.h"
#include "Common/BuildVersion.h"
#include "AudioSlider.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Definition/Level/LevelDefinition.h"
#include "Engine/Font.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Lobby/LobbyRuntimeSubsystem.h"
#include "Engine/GameInstance.h"
#include "Localization/MenuLocalizationSubsystem.h"
#include "Profile/PlayerProfileSubsystem.h"
#include "Online/OnlineSessionsSubsystem.h"
#include "TimerManager.h"
#include "UI/Guide/GuideWidget.h"
#include "UI/Settings/AudioVolumeControl.h"
#include "UI/Record/RecordWidget.h"
#include "UI/Core/WidgetClassDefinition.h"
#include "UI/Shop/ShopWidget.h"
#include "UI/Core/UiSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TitleWidget)

DEFINE_LOG_CATEGORY_STATIC(LogTitleWidget, Log, All);

namespace
{
	const FName LunaChatHintKey(TEXT("Title.LunaChatHint"));
	const FName BossRaidClosedKey(TEXT("Title.BossRaidClosed"));
	const FName BossRaidNoAttemptsKey(TEXT("Title.BossRaidNoAttempts"));
	const FName BossRaidHealthKey(TEXT("Title.BossRaidHealth"));
	const FName BossRaidAttemptsLeftKey(TEXT("Title.BossRaidAttemptsLeft"));
	const FName BossRaidMyRankKey(TEXT("Title.BossRaidMyRank"));

	// 말풍선 폭에서 꼬리가 놓이는 위치. 꼬리 끝은 Luna 머리에 두고 몸통은 주로 왼쪽으로 커지므로,
	// 폭을 다 채운 답(줄바꿈 420 + 여백)도 오른쪽 위 게임 설정 버튼 앞에서 끝난다.
	constexpr float LunaSpeechTailFraction = 0.75f;
}

void UTitleWidget::SetTitleCharacterMaterial(UMaterialInterface* Material)
{
	if (Img_TitleCharacter)
	{
		Img_TitleCharacter->SetBrushFromMaterial(Material);
		Img_TitleCharacter->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

bool UTitleWidget::ShowLunaSpeech(const FName TextKey, const FVector2D& HeadTopUV)
{
	if (!ShowLunaSpeechText(MenuText(TextKey), HeadTopUV))
	{
		return false;
	}
	LunaSpeechKey = TextKey;
	return true;
}

bool UTitleWidget::ShowLunaSpeechText(const FText& Text, const FVector2D& HeadTopUV)
{
	UCanvasPanelSlot* BubbleSlot = LunaSpeechBubble ? Cast<UCanvasPanelSlot>(LunaSpeechBubble->Slot) : nullptr;
	const UCanvasPanelSlot* PortraitSlot = Img_TitleCharacter ? Cast<UCanvasPanelSlot>(Img_TitleCharacter->Slot) : nullptr;
	if (!BubbleSlot || !PortraitSlot || !LunaSpeechText)
	{
		return false;
	}

	LunaSpeechKey = NAME_None;
	LunaSpeechText->SetText(Text);

	// 말풍선은 초상화와 앵커가 같아서, 초상화 배치로 머리 위치를 바로 옮길 수 있다.
	const FVector2D PortraitSize = PortraitSlot->GetSize();
	const FVector2D PortraitTopLeft = PortraitSlot->GetPosition() - PortraitSlot->GetAlignment() * PortraitSize;
	BubbleSlot->SetPosition(PortraitTopLeft + HeadTopUV * PortraitSize);
	LunaSpeechBubble->SetVisibility(ESlateVisibility::HitTestInvisible);
	return true;
}

void UTitleWidget::HideLunaSpeech()
{
	LunaSpeechKey = NAME_None;
	if (LunaSpeechBubble)
	{
		LunaSpeechBubble->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UTitleWidget::OnMenuLanguageChanged()
{
	Super::OnMenuLanguageChanged();
	if (LunaSpeechText && !LunaSpeechKey.IsNone())
	{
		LunaSpeechText->SetText(MenuText(LunaSpeechKey));
	}
	if (LunaChatInput)
	{
		LunaChatInput->SetHintText(MenuText(LunaChatHintKey));
	}
	ApplyBossRaidAvailability();
}

void UTitleWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// NativeConstruct 전에 만들어, 번역 글꼴 적용이 말풍선 텍스트에도 닿게 한다.
	BuildLunaSpeechBubble();
	BuildLunaChatInput();
	BuildVersionLabel();

	// 브라우저를 열 수 없는 플랫폼에서는 웹사이트 버튼을 보이지 않는다.
	if (Btn_Website && !FPlatformProcess::CanLaunchURL(*OfficialWebsiteUrl))
	{
		Btn_Website->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UTitleWidget::SetBossRaidOpen(const bool bOpen)
{
	bBossRaidOpen = bOpen;
	ApplyBossRaidAvailability();
}

void UTitleWidget::SetBossRaidJoining(const bool bJoining)
{
	bBossRaidJoining = bJoining;
	ApplyBossRaidAvailability();
}

// 보스 레이드가 닫혀 있으면 버튼을 끄고, 마우스를 올리면 아직 열리지 않았다고 알린다.
// 기반 클래스가 언어를 바꿀 때 툴팁 연결(버튼 이름)을 다시 적용하므로, 그 뒤에 다시 덮어쓴다.
void UTitleWidget::ApplyBossRaidAvailability()
{
	if (!Btn_BossRaid)
	{
		return;
	}
	// 기록을 받기 전에는 남은 도전이 있다고 보고, 참가 요청에서 백엔드가 다시 확인한다.
	const bool bAttemptsLeft = !BossRaidStanding.bReceived || BossRaidStanding.AttemptsLeft > 0;
	Btn_BossRaid->SetIsEnabled(bBossRaidOpen && bAttemptsLeft && !bBossRaidJoining);
	const FName* OpenTooltipKey = MenuTooltipBindings.Find(Btn_BossRaid->GetFName());
	const FText Availability = !bBossRaidOpen
		? MenuText(BossRaidClosedKey)
		: !bAttemptsLeft
		? MenuText(BossRaidNoAttemptsKey)
		: OpenTooltipKey ? MenuText(*OpenTooltipKey) : FText::GetEmpty();
	FText Tooltip = Availability;
	if (const FText Details = BuildBossRaidDetails(); !Details.IsEmpty())
	{
		Tooltip = Tooltip.IsEmpty() ? Details : FText::Format(INVTEXT("{0}\n\n{1}"), Tooltip, Details);
	}
	SetHoverTooltip(Btn_BossRaid, Tooltip);
}

void UTitleWidget::SetBossRaidDetails(const FBackendBossRaidStatus& Status, const FBackendBossRaidStanding& Standing)
{
	BossRaidStatus = Status;
	BossRaidStanding = Standing;
	ApplyBossRaidAvailability();
}

// 보스 체력 63.20%
// 오늘 남은 도전 2회 · 내 순위 3위 (12,345)
// 1. 이름  120,000 ...
FText UTitleWidget::BuildBossRaidDetails() const
{
	if (!BossRaidStatus.bHasRaid)
	{
		return FText::GetEmpty();
	}

	TArray<FText> Lines;
	FNumberFormattingOptions PercentFormat;
	PercentFormat.MinimumFractionalDigits = 2;
	PercentFormat.MaximumFractionalDigits = 2;
	const double HealthPercent = BossRaidStatus.MaxHealth > 0 ? 100.0 * BossRaidStatus.Health / BossRaidStatus.MaxHealth : 0.0;
	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("Percent"), FText::AsNumber(HealthPercent, &PercentFormat));
	Arguments.Add(TEXT("Attempts"), FText::AsNumber(BossRaidStanding.AttemptsLeft));
	Arguments.Add(TEXT("Rank"), FText::AsNumber(BossRaidStanding.Rank));
	Arguments.Add(TEXT("Damage"), FText::AsNumber(BossRaidStanding.Damage));
	Lines.Add(FText::Format(MenuText(BossRaidHealthKey), Arguments));
	if (BossRaidStanding.bReceived)
	{
		FText Standing = FText::Format(MenuText(BossRaidAttemptsLeftKey), Arguments);
		if (BossRaidStanding.Rank > 0)
		{
			Standing = FText::Format(INVTEXT("{0} · {1}"), Standing, FText::Format(MenuText(BossRaidMyRankKey), Arguments));
		}
		Lines.Add(Standing);
		for (const FBackendBossRaidRankEntry& Entry : BossRaidStanding.Top)
		{
			Lines.Add(FText::Format(INVTEXT("{0}. {1}  {2}"),
				FText::AsNumber(Entry.Rank), FText::FromString(Entry.Name), FText::AsNumber(Entry.Damage)));
		}
	}
	return FText::Join(INVTEXT("\n"), Lines);
}

void UTitleWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);

	ApplyWidgetDefinitionSettings();
	ApplyBossRaidAvailability();

	LoadLocalProfile();

	AudioVolumeControl = NewObject<UAudioVolumeControl>(this);
	AudioVolumeControl->Initialize(this, AudioVolumeSlider_, Btn_Sound);

	ButtonClick::Bind(this, GetMenuButtonBindings());

	if (UOnlineSessionsSubsystem* OnlineSessionsSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UOnlineSessionsSubsystem>()
		: nullptr)
	{
		if (!QuickMatchRequestCompleteHandle.IsValid())
		{
			QuickMatchRequestCompleteHandle = OnlineSessionsSubsystem->OnQuickMatchRequestComplete.AddUObject(this,
				&ThisClass::HandleQuickMatchRequestComplete);
		}
	}
}

void UTitleWidget::NativeDestruct()
{
	if (AudioVolumeControl)
	{
		AudioVolumeControl->Shutdown();
		AudioVolumeControl = nullptr;
	}

	ButtonClick::Unbind(this, GetMenuButtonBindings());

	ClearQuickMatchDelegates();
	if (ShopWidget)
	{
		DismissMenuPopup(ShopWidget);
		ShopWidget = nullptr;
	}
	if (IsValid(GuideWidget))
	{
		DismissMenuPopup(GuideWidget);
	}
	GuideWidget = nullptr;
	if (IsValid(RecordWidget))
	{
		DismissMenuPopup(RecordWidget);
	}
	RecordWidget = nullptr;

	Super::NativeDestruct();
}

TArray<FPdButtonClickBinding, TInlineAllocator<10>> UTitleWidget::GetMenuButtonBindings() const
{
	return {
		{ Btn_RoomList, GET_FUNCTION_NAME_CHECKED(ThisClass, HandleRoomListClicked) },
		{ Btn_QuickMatch, GET_FUNCTION_NAME_CHECKED(ThisClass, HandleQuickMatchClicked) },
		{ Btn_TrainingMode, GET_FUNCTION_NAME_CHECKED(ThisClass, HandleTrainingModeClicked) },
		{ Btn_BossRaid, GET_FUNCTION_NAME_CHECKED(ThisClass, HandleBossRaidClicked) },
		{ Btn_Website, GET_FUNCTION_NAME_CHECKED(ThisClass, HandleWebsiteClicked) },
		{ Btn_PandoraShop, GET_FUNCTION_NAME_CHECKED(ThisClass, HandlePandoraShopClicked) },
		{ Btn_Guide, GET_FUNCTION_NAME_CHECKED(ThisClass, HandleGuideClicked) },
		{ Btn_Record, GET_FUNCTION_NAME_CHECKED(ThisClass, HandleRecordClicked) },
		{ Btn_Exit, GET_FUNCTION_NAME_CHECKED(ThisClass, HandleExitClicked) },
		{ Btn_Tutorial, GET_FUNCTION_NAME_CHECKED(ThisClass, HandleGuideClicked) },
	};
}

void UTitleWidget::ApplyWidgetDefinitionSettings()
{
	if (const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this))
	{
		const FTitleAuxiliaryWidgetSettings& Settings = WidgetDefinition->GetTitleAuxiliaryWidgetSettings();
		if (const TSubclassOf<UShopWidget> ResolvedShopWidgetClass = WidgetDefinition->GetShopWidgetClass())
		{
			ShopWidgetClass = ResolvedShopWidgetClass;
		}
		if (const TSubclassOf<UGuideWidget> ResolvedGuideWidgetClass = WidgetDefinition->GetGuideWidgetClass())
		{
			GuideWidgetClass = ResolvedGuideWidgetClass;
		}
		if (const TSubclassOf<URecordWidget> ResolvedRecordWidgetClass = WidgetDefinition->GetRecordWidgetClass())
		{
			RecordWidgetClass = ResolvedRecordWidgetClass;
		}
		QuickMatchMaxSearchResults = FMath::Max(Settings.QuickMatchMaxSearchResults, 1);
		QuickMatchMaxPublicConnections = FMath::Max(Settings.QuickMatchMaxPublicConnections, 1);
		QuickMatchRoomName = Settings.QuickMatchRoomName;
		bQuickMatchLAN = Settings.bQuickMatchLAN;
		bQuickMatchUseLobbies = Settings.bQuickMatchUseLobbies;
	}
}

// 화면 오른쪽 아래에 빌드 버전을 작게 둔다. 버그 제보나 스크린샷만 보고도 어느 빌드인지 알 수 있게 한다.
// 루트는 반응형 크기 조절용 래퍼(ScaleBox → SizeBox)라서, 그 안의 첫 캔버스(디자인 해상도)에 붙여 함께 크기가 바뀌게 한다.
void UTitleWidget::BuildVersionLabel()
{
	UWidget* Widget = WidgetTree ? WidgetTree->RootWidget.Get() : nullptr;
	while (Widget && !Widget->IsA<UCanvasPanel>())
	{
		const UPanelWidget* Wrapper = Cast<UPanelWidget>(Widget);
		Widget = Wrapper && Wrapper->GetChildrenCount() > 0 ? Wrapper->GetChildAt(0) : nullptr;
	}
	UCanvasPanel* Canvas = Cast<UCanvasPanel>(Widget);
	if (!Canvas)
	{
		return;
	}
	UTextBlock* VersionText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("VersionLabel"));
	VersionText->SetText(FText::AsCultureInvariant(PdBuildVersion::GetDisplayString()));
	VersionText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.55f)));
	FSlateFontInfo Font = VersionText->GetFont();
	Font.Size = 12;
	VersionText->SetFont(Font);
	VersionText->SetVisibility(ESlateVisibility::HitTestInvisible);
	UCanvasPanelSlot* LabelSlot = Canvas->AddChildToCanvas(VersionText);
	LabelSlot->SetAnchors(FAnchors(1.f, 1.f));
	LabelSlot->SetAlignment(FVector2D(1.f, 1.f));
	LabelSlot->SetPosition(FVector2D(-16.f, -12.f));
	LabelSlot->SetAutoSize(true);
}

void UTitleWidget::BuildLunaSpeechBubble()
{
	// Luna 초상화와 같은 캔버스에서 한 층 위에 두어, 말풍선이 타이틀의 반응형 크기를 따른다.
	UCanvasPanel* Canvas = Img_TitleCharacter ? Cast<UCanvasPanel>(Img_TitleCharacter->GetParent()) : nullptr;
	const UCanvasPanelSlot* PortraitSlot = Img_TitleCharacter ? Cast<UCanvasPanelSlot>(Img_TitleCharacter->Slot) : nullptr;
	if (!Canvas || !PortraitSlot || !WidgetTree || LunaSpeechBubble)
	{
		return;
	}

	// 색은 타이틀 메뉴를 따른다: 패널 외곽선과 안내 문구 색.
	FSlateBrush TailBrush;
	TailBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	TailBrush.TintColor = FSlateColor(FLinearColor(0.955f, 0.896f, 0.776f));
	TailBrush.OutlineSettings.Color = FSlateColor(FLinearColor(0.701f, 0.474f, 0.195f));
	TailBrush.OutlineSettings.Width = 2.f;
	TailBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
	FSlateBrush BodyBrush = TailBrush;
	BodyBrush.OutlineSettings.CornerRadii = FVector4(14.f, 14.f, 14.f, 14.f);

	UOverlay* Bubble = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("LunaSpeechBubble"));

	// 몸통 뒤에 회전한 정사각형을 두고 아래 절반만 보여 Luna를 가리키는 꼬리로 쓴다.
	// 양쪽 여백 위젯이 글 길이와 상관없이 꼬리를 폭의 LunaSpeechTailFraction 위치에 둔다.
	UHorizontalBox* TailRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("LunaSpeechTailRow"));
	UImage* Tail = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("LunaSpeechTail"));
	Tail->SetBrush(TailBrush);
	Tail->SetDesiredSizeOverride(FVector2D(20.f, 20.f));
	Tail->SetRenderTransformAngle(45.f);
	auto AddTailSpacer = [this, TailRow](const float Fill)
	{
		FSlateChildSize Size(ESlateSizeRule::Fill);
		Size.Value = Fill;
		TailRow->AddChildToHorizontalBox(WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass()))->SetSize(Size);
	};
	AddTailSpacer(LunaSpeechTailFraction);
	UHorizontalBoxSlot* TailCell = TailRow->AddChildToHorizontalBox(Tail);
	TailCell->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	TailCell->SetVerticalAlignment(VAlign_Bottom);
	AddTailSpacer(1.f - LunaSpeechTailFraction);
	UOverlaySlot* TailSlot = Bubble->AddChildToOverlay(TailRow);
	TailSlot->SetHorizontalAlignment(HAlign_Fill);
	TailSlot->SetVerticalAlignment(VAlign_Bottom);
	TailSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));

	UBorder* Body = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("LunaSpeechBody"));
	Body->SetBrush(BodyBrush);
	Body->SetPadding(FMargin(22.f, 14.f));
	UOverlaySlot* BodySlot = Bubble->AddChildToOverlay(Body);
	BodySlot->SetHorizontalAlignment(HAlign_Fill);
	BodySlot->SetVerticalAlignment(VAlign_Fill);
	BodySlot->SetPadding(FMargin(0.f, 0.f, 0.f, 14.f));

	LunaSpeechText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Txt_LunaSpeech"));
	FSlateFontInfo Font = LunaSpeechText->GetFont();
	Font.Size = 18;
	LunaSpeechText->SetFont(Font);
	LunaSpeechText->SetColorAndOpacity(FSlateColor(FLinearColor(0.130f, 0.056f, 0.021f)));
	LunaSpeechText->SetWrapTextAt(420.f);
	Body->SetContent(LunaSpeechText);

	UCanvasPanelSlot* BubbleSlot = Canvas->AddChildToCanvas(Bubble);
	BubbleSlot->SetAutoSize(true);
	BubbleSlot->SetAnchors(PortraitSlot->GetAnchors());
	BubbleSlot->SetAlignment(FVector2D(LunaSpeechTailFraction, 1.f)); // 꼬리 끝을 머리 위치에 둔다.
	BubbleSlot->SetZOrder(PortraitSlot->GetZOrder() + 1);

	Bubble->SetVisibility(ESlateVisibility::Collapsed);
	LunaSpeechBubble = Bubble;
}

void UTitleWidget::BuildLunaChatInput()
{
	// 초상화와 같은 캔버스·앵커로 아래 가장자리에 두어, 어떤 해상도에서도 상자가 Luna 아래에 있다.
	UCanvasPanel* Canvas = Img_TitleCharacter ? Cast<UCanvasPanel>(Img_TitleCharacter->GetParent()) : nullptr;
	const UCanvasPanelSlot* PortraitSlot = Img_TitleCharacter ? Cast<UCanvasPanelSlot>(Img_TitleCharacter->Slot) : nullptr;
	if (!Canvas || !PortraitSlot || !WidgetTree || LunaChatInput)
	{
		return;
	}

	LunaChatInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("Input_LunaChat"));

	// UE 5.8의 SetWidgetStyle은 Slate 위젯이 생긴 뒤에는 인자의 주소를 Slate에 넘긴다.
	// 여기서는 Slate 위젯이 아직 없으므로 스타일을 UMG 멤버에만 복사한다.
	FSlateBrush Background;
	Background.DrawAs = ESlateBrushDrawType::RoundedBox;
	Background.TintColor = FSlateColor(FLinearColor(0.955f, 0.896f, 0.776f));
	Background.OutlineSettings.Color = FSlateColor(FLinearColor(0.701f, 0.474f, 0.195f));
	Background.OutlineSettings.Width = 2.f;
	Background.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
	Background.OutlineSettings.CornerRadii = FVector4(12.f, 12.f, 12.f, 12.f);
	FSlateBrush FocusedBackground = Background;
	FocusedBackground.OutlineSettings.Color = FSlateColor(FLinearColor(0.905f, 0.640f, 0.260f));
	FocusedBackground.OutlineSettings.Width = 3.f;

	// 플레이어는 어떤 언어로든 입력할 수 있으므로, 메뉴 언어와 상관없이 라틴 문자도 담은 CJK 글꼴을 쓴다.
	FEditableTextBoxStyle Style = LunaChatInput->GetWidgetStyle();
	FSlateFontInfo Font = Style.TextStyle.Font;
	const UGameInstance* GameInstance = GetGameInstance();
	if (const UMenuLocalizationSubsystem* Localization = GameInstance ? GameInstance->GetSubsystem<UMenuLocalizationSubsystem>() : nullptr)
	{
		if (UFont* CjkFont = Localization->GetCjkFont())
		{
			Font.FontObject = CjkFont;
			Font.TypefaceFontName = NAME_None;
		}
	}
	Font.Size = 16;
	const FSlateColor TextColor(FLinearColor(0.130f, 0.056f, 0.021f));
	Style.SetBackgroundImageNormal(Background).SetBackgroundImageHovered(Background)
		.SetBackgroundImageFocused(FocusedBackground).SetBackgroundImageReadOnly(Background)
		.SetPadding(FMargin(16.f, 10.f)).SetFont(Font).SetForegroundColor(TextColor)
		.SetFocusedForegroundColor(TextColor);
	LunaChatInput->SetWidgetStyle(Style);
	LunaChatInput->SetHintText(MenuText(LunaChatHintKey));
	LunaChatInput->SetClearKeyboardFocusOnCommit(false);
	LunaChatInput->OnTextCommitted.AddDynamic(this, &ThisClass::HandleLunaChatCommitted);

	const FVector2D PortraitSize = PortraitSlot->GetSize();
	const FVector2D PortraitTopLeft = PortraitSlot->GetPosition() - PortraitSlot->GetAlignment() * PortraitSize;
	UCanvasPanelSlot* InputSlot = Canvas->AddChildToCanvas(LunaChatInput);
	InputSlot->SetAnchors(PortraitSlot->GetAnchors());
	InputSlot->SetAlignment(FVector2D(0.5f, 1.f));
	InputSlot->SetPosition(PortraitTopLeft + FVector2D(PortraitSize.X * 0.5f, PortraitSize.Y - 24.f));
	InputSlot->SetSize(FVector2D(FMath::Min(420.f, PortraitSize.X * 0.9f), 48.f));
	InputSlot->SetZOrder(PortraitSlot->GetZOrder() + 2);
}

void UTitleWidget::HandleLunaChatCommitted(const FText& Text, const ETextCommit::Type CommitMethod)
{
	const FString Question = Text.ToString().TrimStartAndEnd();
	if (CommitMethod != ETextCommit::OnEnter || Question.IsEmpty())
	{
		return;
	}

	LunaChatInput->SetText(FText::GetEmpty());
	LunaQuestionSubmitted.Broadcast(Question);
}

void UTitleWidget::HandleRoomListClicked()
{
	OpenRoomList();
}

void UTitleWidget::HandleQuickMatchClicked()
{
	StartQuickMatch();
}

void UTitleWidget::HandleTrainingModeClicked()
{
	OpenTrainingRoom();
}

void UTitleWidget::HandleBossRaidClicked()
{
	BossRaidRequested.Broadcast();
}

void UTitleWidget::HandleWebsiteClicked()
{
	FString Error;
	FPlatformProcess::LaunchURL(*OfficialWebsiteUrl, nullptr, &Error);
	UE_CLOG(!Error.IsEmpty(), LogTitleWidget, Warning, TEXT("Could not open the official website %s: %s"), *OfficialWebsiteUrl, *Error);
}

void UTitleWidget::HandlePandoraShopClicked()
{
	OpenShop();
}

void UTitleWidget::HandleGuideClicked()
{
	OpenGuide();
}

void UTitleWidget::HandleRecordClicked()
{
	OpenRecord();
}

void UTitleWidget::HandleRecordCloseClicked()
{
	if (!IsValid(RecordWidget))
	{
		RecordWidget = nullptr;
		return;
	}

	RecordWidget->DeactivateWidget();
}

void UTitleWidget::HandleExitClicked()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, true);
}

FString UTitleWidget::GetResolvedLobbyTravelMapName() const
{
	const ULevelDefinition* Definition = ULevelDefinition::ResolveDefaultDefinition();
	return Definition ? Definition->GetLobbyTravelMapName() : FString();
}

FString UTitleWidget::GetResolvedRoomTravelMapName() const
{
	const ULevelDefinition* Definition = ULevelDefinition::ResolveDefaultDefinition();
	return Definition ? Definition->GetRoomTravelMapName() : FString();
}

FString UTitleWidget::GetResolvedTrainingRoomTravelMapName() const
{
	const ULevelDefinition* Definition = ULevelDefinition::ResolveDefaultDefinition();
	return Definition ? Definition->GetTrainingRoomTravelMapName() : FString();
}

void UTitleWidget::OpenRoomList()
{
	const FString RoomMapName = GetResolvedRoomTravelMapName();
	if (RoomMapName.IsEmpty())
	{
		return;
	}

	EditorTransaction::ResetIfContainsPieObjects();
	UGameplayStatics::OpenLevel(this, FName(*RoomMapName));
}

void UTitleWidget::OpenTrainingRoom()
{
	const FString TrainingRoomMapName = GetResolvedTrainingRoomTravelMapName();
	if (TrainingRoomMapName.IsEmpty())
	{
		return;
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (ULobbyRuntimeSubsystem* LobbyRuntimeSubsystem = GameInstance->GetSubsystem<ULobbyRuntimeSubsystem>())
		{
			LobbyRuntimeSubsystem->BeginGameEntryContentPreload();
		}
	}
	EditorTransaction::ResetIfContainsPieObjects();
	UGameplayStatics::OpenLevel(this, FName(*TrainingRoomMapName));
}

void UTitleWidget::OpenShop()
{
	LoadLocalProfile();
	EditorTransaction::ResetIfContainsPieObjects();

	const TSubclassOf<UShopWidget> ResolvedShopWidgetClass = ResolveShopWidgetClass();
	if (!ResolvedShopWidgetClass)
	{
		return;
	}

	APlayerController* PlayerController = GetOwningPlayer();
	if (!PlayerController)
	{
		return;
	}

	if (!ShopWidget || ShopWidget->GetClass() != ResolvedShopWidgetClass.Get())
	{
		if (ShopWidget)
		{
			DismissMenuPopup(ShopWidget);
			ShopWidget = nullptr;
		}

		ShopWidget = CreateWidget<UShopWidget>(PlayerController, ResolvedShopWidgetClass);
	}

	if (!ShopWidget)
	{
		return;
	}

	ShopWidget->RefreshUI();
	PresentMenuPopup(ShopWidget, FSimpleDelegate());
}

void UTitleWidget::OpenGuide()
{
	EditorTransaction::ResetIfContainsPieObjects();

	APlayerController* PlayerController = GetOwningPlayer();
	if (!PlayerController)
	{
		return;
	}

	const TSubclassOf<UGuideWidget> ResolvedGuideWidgetClass = ResolveGuideWidgetClass();
	if (!ResolvedGuideWidgetClass)
	{
		return;
	}

	if (GuideWidget && (!IsValid(GuideWidget) || GuideWidget->GetWorld() != GetWorld()))
	{
		GuideWidget = nullptr;
	}

	if (!GuideWidget || GuideWidget->GetClass() != ResolvedGuideWidgetClass.Get())
	{
		if (GuideWidget)
		{
			DismissMenuPopup(GuideWidget);
			GuideWidget = nullptr;
		}

		GuideWidget = CreateWidget<UGuideWidget>(PlayerController, ResolvedGuideWidgetClass);
	}

	if (!GuideWidget)
	{
		return;
	}

	GuideWidget->RefreshGuide();

	PresentMenuPopup(GuideWidget, FSimpleDelegate::CreateWeakLambda(this, [this]()
	{
		if (IsValid(GuideWidget))
		{
			GuideWidget->CloseGuide();
		}
	}));
}

void UTitleWidget::OpenRecord()
{
	EditorTransaction::ResetIfContainsPieObjects();

	APlayerController* PlayerController = GetOwningPlayer();
	if (!PlayerController)
	{
		return;
	}

	const TSubclassOf<URecordWidget> ResolvedRecordWidgetClass = ResolveRecordWidgetClass();
	if (!ResolvedRecordWidgetClass)
	{
		return;
	}

	if (RecordWidget && (!IsValid(RecordWidget) || RecordWidget->GetWorld() != GetWorld()))
	{
		RecordWidget = nullptr;
	}

	if (!RecordWidget || RecordWidget->GetClass() != ResolvedRecordWidgetClass.Get())
	{
		if (RecordWidget)
		{
			DismissMenuPopup(RecordWidget);
			RecordWidget = nullptr;
		}

		RecordWidget = CreateWidget<URecordWidget>(PlayerController, ResolvedRecordWidgetClass);
	}

	if (!RecordWidget)
	{
		return;
	}

	PresentMenuPopup(RecordWidget, FSimpleDelegate::CreateUObject(this, &ThisClass::HandleRecordCloseClicked));

	RecordWidget->RefreshRecords();
}

TSubclassOf<UShopWidget> UTitleWidget::ResolveShopWidgetClass() const
{
	if (ShopWidgetClass)
	{
		return ShopWidgetClass;
	}

	if (const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this))
	{
		return WidgetDefinition->GetShopWidgetClass();
	}

	return nullptr;
}

TSubclassOf<UGuideWidget> UTitleWidget::ResolveGuideWidgetClass() const
{
	if (GuideWidgetClass)
	{
		return GuideWidgetClass;
	}

	if (const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this))
	{
		return WidgetDefinition->GetGuideWidgetClass();
	}

	return nullptr;
}

TSubclassOf<URecordWidget> UTitleWidget::ResolveRecordWidgetClass() const
{
	if (RecordWidgetClass)
	{
		return RecordWidgetClass;
	}

	if (const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this))
	{
		return WidgetDefinition->GetRecordWidgetClass();
	}

	return nullptr;
}

void UTitleWidget::LoadLocalProfile() const
{
	UPlayerProfileSubsystem* ProfileSubsystem = UGameInstance::GetSubsystem<UPlayerProfileSubsystem>(GetGameInstance());
	if (!ProfileSubsystem)
	{
		return;
	}

	ProfileSubsystem->LoadProfile();
}

void UTitleWidget::StartQuickMatch()
{
	if (ActiveQuickMatchRequestId != 0)
	{
		return;
	}
	SetQuickMatchEnabled(false);
	UOnlineSessionsSubsystem* OnlineSessionsSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UOnlineSessionsSubsystem>()
		: nullptr;
	if (!OnlineSessionsSubsystem)
	{
		SetQuickMatchEnabled(true);

		return;
	}

	if (!QuickMatchRequestCompleteHandle.IsValid())
	{
		QuickMatchRequestCompleteHandle = OnlineSessionsSubsystem->OnQuickMatchRequestComplete.AddUObject(this,
			&ThisClass::HandleQuickMatchRequestComplete);
	}

	const uint64 RequestId = OnlineSessionsSubsystem->BeginQuickMatch(GetOwningLocalPlayer(),
		QuickMatchMaxSearchResults, QuickMatchMaxPublicConnections, QuickMatchRoomName, TEXT("Lobby"), bQuickMatchLAN,
		bQuickMatchUseLobbies);
	if (RequestId == 0)
	{
		SetQuickMatchEnabled(true);
	}
	else
	{
		ActiveQuickMatchRequestId = RequestId;
	}
}

void UTitleWidget::OpenLobbyAsListenServer() const
{
	const FString LobbyMapName = GetResolvedLobbyTravelMapName();
	if (LobbyMapName.IsEmpty())
	{
		return;
	}

	EditorTransaction::ResetIfContainsPieObjects();
	if (const UOnlineSessionsSubsystem* OnlineSessionsSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UOnlineSessionsSubsystem>()
		: nullptr)
	{
		OnlineSessionsSubsystem->TravelToListenMap(LobbyMapName);
	}
}

void UTitleWidget::SetQuickMatchEnabled(const bool bEnabled) const
{
	if (Btn_QuickMatch)
	{
		Btn_QuickMatch->SetIsEnabled(bEnabled);
	}
}

void UTitleWidget::ClearQuickMatchDelegates()
{
	UOnlineSessionsSubsystem* OnlineSessionsSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UOnlineSessionsSubsystem>()
		: nullptr;
	if (!OnlineSessionsSubsystem)
	{
		QuickMatchRequestCompleteHandle.Reset();
		ActiveQuickMatchRequestId = 0;
		return;
	}

	if (QuickMatchRequestCompleteHandle.IsValid())
	{
		OnlineSessionsSubsystem->OnQuickMatchRequestComplete.Remove(QuickMatchRequestCompleteHandle);
		QuickMatchRequestCompleteHandle.Reset();
	}

	const uint64 RequestId = ActiveQuickMatchRequestId;
	ActiveQuickMatchRequestId = 0;
	if (RequestId != 0)
	{
		OnlineSessionsSubsystem->CancelSessionRequest(RequestId);
	}
}

void UTitleWidget::PresentMenuPopup(UPdScreenWidget* Popup, FSimpleDelegate OnBack)
{
	Popup->SetVisibility(ESlateVisibility::Visible);
	if (Popup->IsActivated())
	{
		return;
	}

	Popup->ConfigureScreen(Popup, MoveTemp(OnBack));
	GetUiSubsystem()->PushScreen(Popup, EUiScreenLayer::Menu);
}

void UTitleWidget::DismissMenuPopup(UPdScreenWidget* Popup) const
{
	Popup->DeactivateWidget();
}

UUiSubsystem* UTitleWidget::GetUiSubsystem() const
{
	const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	return LocalPlayer ? LocalPlayer->GetSubsystem<UUiSubsystem>() : nullptr;
}

void UTitleWidget::HandleQuickMatchRequestComplete(const uint64 RequestId, const bool bWasSuccessful,
	const bool bCreatedRoom)
{
	if (RequestId == 0 || RequestId != ActiveQuickMatchRequestId)
	{
		return;
	}

	ActiveQuickMatchRequestId = 0;

	if (bWasSuccessful)
	{
		if (bCreatedRoom)
		{
			OpenLobbyAsListenServer();
		}
		return;
	}

	SetQuickMatchEnabled(true);
}
