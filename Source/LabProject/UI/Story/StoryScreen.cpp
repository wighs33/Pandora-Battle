#include "UI/Story/StoryScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Localization/MenuLocalizationSubsystem.h"
#include "Sound/SoundBase.h"
#include "Story/StoryCharacterDefinition.h"
#include "Story/StorySettings.h"
#include "UI/Common/UiMotion.h"
#include "UI/Core/GameplayScreen.h"
#include "UI/Core/UiSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StoryScreen)

namespace
{
	// StoryText 표의 키
	const FName AdvanceHintKey(TEXT("Story.Hint.Advance"));

	// 1920 x 1080 기준
	constexpr float LetterboxHeight = 92.f;
	constexpr float LetterboxSpeed = 3.2f;
	constexpr float ShadeHeight = 460.f;
	constexpr float LineBottom = 122.f;
	constexpr float MaxLineWidth = 1240.f;
	constexpr float LineWidthSlack = 8.f;
	constexpr int32 NameSize = 26;
	constexpr int32 LineSizeNormal = 32;
	constexpr int32 LineSizeShout = 42;
	constexpr float NextMarkSize = 26.f;
	constexpr float HintRight = 64.f;
	constexpr float HintBottom = 34.f;

	constexpr float CharactersPerSecond = 34.f;
	constexpr float ShoutSpeedScale = 1.7f;
	constexpr float PunctuationPause = 0.16f;
	constexpr int32 CharactersPerVoice = 2;
	constexpr float VoiceVolume = 0.55f;
	constexpr float VoicePitchJitter = 0.06f;
	/** 대사가 나타나며 떠오르는 높이와 시간 */
	constexpr float LineRise = 14.f;
	constexpr float LineRiseSeconds = 0.18f;

	/** 넘김 표시가 깜빡이는 빠르기, 가장 흐릴 때의 진하기, 오르내리는 폭 */
	constexpr float NextMarkSpeed = 4.5f;
	constexpr float NextMarkMinOpacity = 0.45f;
	constexpr float NextMarkBob = 4.f;

	/** 이만큼 뜨면 눈꺼풀을 감춘다 */
	constexpr float EyelidHiddenOpenness = 0.995f;
	/** 다 떴을 때 눈꺼풀이 움직인 거리(화면 높이의 비율) */
	constexpr float EyelidTravel = 0.95f;

	constexpr float TitleFadeSeconds = 0.8f;
	constexpr float MashBarWidth = 360.f;
	constexpr float MashBarHeight = 14.f;

	const FLinearColor IvoryColor(0.94f, 0.91f, 0.84f, 1.f);
	const FLinearColor GoldColor(0.84f, 0.66f, 0.36f, 1.f);
	const FLinearColor HintColor(0.94f, 0.91f, 0.84f, 0.55f);
	const FLinearColor ShadowColor(0.f, 0.f, 0.f, 0.85f);

	bool IsAdvanceKey(const FKey& Key)
	{
		return Key == EKeys::SpaceBar || Key == EKeys::Enter || Key == EKeys::E || Key == EKeys::Gamepad_FaceButton_Bottom;
	}

	UCanvasPanelSlot* PlaceInCanvas(UCanvasPanel& Canvas, UWidget& Widget, const FAnchors& Anchors, const FVector2D& Alignment, const FVector2D& Position)
	{
		UCanvasPanelSlot* Slot = Canvas.AddChildToCanvas(&Widget);
		Slot->SetAnchors(Anchors);
		Slot->SetAlignment(Alignment);
		Slot->SetAutoSize(true);
		Slot->SetPosition(Position);
		return Slot;
	}
}

UStoryScreen::UStoryScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsBackHandler = true;
	SetIsFocusable(true);
}

UStoryScreen* UStoryScreen::Create(APlayerController* OwningPlayer, FSimpleDelegate InOnBack)
{
	UStoryScreen* Screen = CreateWidget<UStoryScreen>(OwningPlayer);
	if (Screen)
	{
		Screen->OnBack = MoveTemp(InOnBack);
	}
	return Screen;
}

void UStoryScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	const UStorySettings* Settings = GetDefault<UStorySettings>();

	// 화면 어디를 눌러도 넘어가게 투명한 바탕이 클릭을 받는다. 커서는 숨긴다.
	Root = WidgetTree->ConstructWidget<UBorder>();
	Root->SetBrush(FSlateColorBrush(FLinearColor::Transparent));
	Root->SetPadding(FMargin(0.f));
	Root->SetCursor(EMouseCursor::None);
	WidgetTree->RootWidget = Root;
	Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	Root->SetContent(Canvas);

	EyelidTop = AddFullScreenImage();
	EyelidTop->SetBrushFromTexture(Settings->EyelidTexture.LoadSynchronous());
	EyelidBottom = AddFullScreenImage();
	EyelidBottom->SetBrushFromTexture(Settings->EyelidTexture.LoadSynchronous());
	EyelidBottom->SetRenderScale(FVector2D(1.f, -1.f));
	ApplyEyelids();

	auto AddBand = [this](const float AnchorY)
	{
		UImage* Band = WidgetTree->ConstructWidget<UImage>();
		Band->SetBrush(FSlateColorBrush(FLinearColor::Black));
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Band);
		Slot->SetAnchors(FAnchors(0.f, AnchorY, 1.f, AnchorY));
		Slot->SetAlignment(FVector2D(0.f, AnchorY));
		Slot->SetOffsets(FMargin(0.f, 0.f, 0.f, LetterboxHeight));
		return Band;
	};
	Shade = AddBand(1.f);
	Shade->SetBrushFromTexture(Settings->DialogueShadeTexture.LoadSynchronous());
	CastChecked<UCanvasPanelSlot>(Shade->Slot)->SetOffsets(FMargin(0.f, 0.f, 0.f, ShadeHeight));
	LetterboxTop = AddBand(0.f);
	LetterboxBottom = AddBand(1.f);

	FadeImage = AddFullScreenImage();
	FadeImage->SetBrush(FSlateColorBrush(FLinearColor::White));

	BuildLine();
	BuildTitleCard();
	BuildMash();

	ObjectiveText = MakeText(28, IvoryColor);
	PlaceInCanvas(*Canvas, *ObjectiveText, FAnchors(0.5f, 0.f), FVector2D(0.5f, 0.f), FVector2D(0.f, 70.f));
	ObjectiveText->SetVisibility(ESlateVisibility::Collapsed);

	Canvas->SetVisibility(ESlateVisibility::HitTestInvisible);
	HideLine();
}

UTextBlock* UStoryScreen::MakeText(const int32 Size, const FLinearColor& Color)
{
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
	FSlateFontInfo Font = Text->GetFont();
	Font.Size = Size;
	Text->SetFont(Font);
	Text->SetColorAndOpacity(Color);
	Text->SetShadowColorAndOpacity(ShadowColor);
	Text->SetShadowOffset(FVector2D(0.f, 2.f));
	if (const UMenuLocalizationSubsystem* Localization = UGameInstance::GetSubsystem<UMenuLocalizationSubsystem>(GetGameInstance()))
	{
		Localization->ApplyLanguageFont(*Text);
	}
	return Text;
}

UImage* UStoryScreen::AddFullScreenImage()
{
	UImage* Image = WidgetTree->ConstructWidget<UImage>();
	UCanvasPanelSlot* ImageSlot = Canvas->AddChildToCanvas(Image);
	ImageSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	ImageSlot->SetOffsets(FMargin(0.f));
	return Image;
}

// 이름, 대사, 넘김 표시를 아래 가운데에 세로로 쌓는다.
void UStoryScreen::BuildLine()
{
	LineBox = WidgetTree->ConstructWidget<UVerticalBox>();
	PlaceInCanvas(*Canvas, *LineBox, FAnchors(0.5f, 1.f), FVector2D(0.5f, 1.f), FVector2D(0.f, -LineBottom));

	NameText = MakeText(NameSize, GoldColor);
	LineBox->AddChildToVerticalBox(NameText)->SetHorizontalAlignment(HAlign_Center);

	// 글자가 나오는 동안 줄이 움직이지 않게, 다 나왔을 때의 폭을 미리 잡고 왼쪽부터 채운다.
	LineSize = WidgetTree->ConstructWidget<USizeBox>();
	LineText = MakeText(LineSizeNormal, IvoryColor);
	LineText->SetAutoWrapText(true);
	LineSize->AddChild(LineText);
	UVerticalBoxSlot* LineSlot = LineBox->AddChildToVerticalBox(LineSize);
	LineSlot->SetHorizontalAlignment(HAlign_Center);
	LineSlot->SetPadding(FMargin(0.f, 10.f, 0.f, 6.f));

	NextMark = WidgetTree->ConstructWidget<UImage>();
	NextMark->SetBrushFromTexture(GetDefault<UStorySettings>()->NextMarkTexture.LoadSynchronous());
	NextMark->SetDesiredSizeOverride(FVector2D(NextMarkSize, NextMarkSize));
	LineBox->AddChildToVerticalBox(NextMark)->SetHorizontalAlignment(HAlign_Center);

	HintText = MakeText(18, HintColor);
	HintText->SetText(UMenuLocalizationSubsystem::GetStoryText(AdvanceHintKey));
	PlaceInCanvas(*Canvas, *HintText, FAnchors(1.f, 1.f), FVector2D(1.f, 1.f), FVector2D(-HintRight, -HintBottom));
}

void UStoryScreen::BuildTitleCard()
{
	TitleBox = WidgetTree->ConstructWidget<UVerticalBox>();
	PlaceInCanvas(*Canvas, *TitleBox, FAnchors(0.5f, 0.5f), FVector2D(0.5f, 0.5f), FVector2D(0.f, -20.f));
	ChapterText = MakeText(26, GoldColor);
	TitleBox->AddChildToVerticalBox(ChapterText)->SetHorizontalAlignment(HAlign_Center);
	TitleText = MakeText(64, IvoryColor);
	UVerticalBoxSlot* TitleSlot = TitleBox->AddChildToVerticalBox(TitleText);
	TitleSlot->SetHorizontalAlignment(HAlign_Center);
	TitleSlot->SetPadding(FMargin(0.f, 14.f, 0.f, 0.f));
	TitleBox->SetRenderOpacity(0.f);
}

void UStoryScreen::BuildMash()
{
	MashBox = WidgetTree->ConstructWidget<UVerticalBox>();
	PlaceInCanvas(*Canvas, *MashBox, FAnchors(0.5f, 1.f), FVector2D(0.5f, 1.f), FVector2D(0.f, -LineBottom - 30.f));
	MashText = MakeText(30, IvoryColor);
	MashBox->AddChildToVerticalBox(MashText)->SetHorizontalAlignment(HAlign_Center);

	USizeBox* BarSize = WidgetTree->ConstructWidget<USizeBox>();
	BarSize->SetWidthOverride(MashBarWidth);
	BarSize->SetHeightOverride(MashBarHeight);
	MashBar = WidgetTree->ConstructWidget<UProgressBar>();
	FProgressBarStyle Style = MashBar->GetWidgetStyle();
	Style.BackgroundImage = FSlateColorBrush(FLinearColor(0.f, 0.f, 0.f, 0.6f));
	Style.FillImage = FSlateColorBrush(FLinearColor::White);
	MashBar->SetWidgetStyle(Style);
	MashBar->SetFillColorAndOpacity(GoldColor);
	BarSize->AddChild(MashBar);
	UVerticalBoxSlot* BarSlot = MashBox->AddChildToVerticalBox(BarSize);
	BarSlot->SetHorizontalAlignment(HAlign_Center);
	BarSlot->SetPadding(FMargin(0.f, 14.f, 0.f, 0.f));
	MashBox->SetVisibility(ESlateVisibility::Collapsed);
}

// 연출 중에는 메뉴 입력이라 게임 입력이 막힌다. 직접 걷는 동안에는 위에 올린 게임 중 기본 화면이 게임 입력을 연다.
TOptional<FUIInputConfig> UStoryScreen::GetDesiredInputConfig() const
{
	FUIInputConfig Config(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
	Config.bIgnoreMoveInput = Config.bIgnoreLookInput = true;
	return Config;
}

UWidget* UStoryScreen::NativeGetDesiredFocusTarget() const
{
	return const_cast<UStoryScreen*>(this);
}

bool UStoryScreen::NativeOnHandleBackAction()
{
	OnBack.ExecuteIfBound();
	return true;
}

FReply UStoryScreen::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (!bPlayerControl && IsAdvanceKey(InKeyEvent.GetKey()))
	{
		if (!InKeyEvent.IsRepeat())
		{
			PressAdvance();
		}
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UStoryScreen::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bPlayerControl && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		PressAdvance();
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UStoryScreen::PressAdvance()
{
	if (bMashing)
	{
		++MashPresses;
		return;
	}
	if (!bLineShown)
	{
		return;
	}
	if (IsTyping())
	{
		TypedCharacters = FullLine.Len();
		LineText->SetText(FText::FromString(FullLine));
		return;
	}
	bLineAdvanced = true;
}

void UStoryScreen::ShowLine(const UStoryCharacterDefinition* Speaker, const FName TextKey, const bool bShout, USoundBase* Voice)
{
	FullLine = UMenuLocalizationSubsystem::GetStoryText(TextKey).ToString();
	TypedCharacters = 0.f;
	TypingPause = 0.f;
	LastVoiceCharacter = 0;
	LineElapsed = 0.f;
	bLineShown = true;
	bLineAdvanced = false;
	VoiceSound = Voice;
	VoicePitch = Speaker ? Speaker->VoicePitch : 1.f;

	NameText->SetText(Speaker ? UMenuLocalizationSubsystem::GetStoryText(Speaker->NameKey) : FText::GetEmpty());
	NameText->SetColorAndOpacity(Speaker ? Speaker->NameColor : GoldColor);
	FSlateFontInfo Font = LineText->GetFont();
	Font.Size = bShout ? LineSizeShout : LineSizeNormal;
	LineText->SetFont(Font);
	LineText->SetText(FText::GetEmpty());
	LineSize->SetWidthOverride(FMath::Min(MeasureLineWidth(FullLine) + LineWidthSlack, MaxLineWidth));

	LineBox->SetVisibility(ESlateVisibility::HitTestInvisible);
	Shade->SetVisibility(ESlateVisibility::HitTestInvisible);
	HintText->SetVisibility(ESlateVisibility::HitTestInvisible);
	NextMark->SetRenderOpacity(0.f);
}

void UStoryScreen::HideLine()
{
	bLineShown = false;
	bLineAdvanced = false;
	LineBox->SetVisibility(ESlateVisibility::Collapsed);
	Shade->SetVisibility(ESlateVisibility::Collapsed);
	HintText->SetVisibility(ESlateVisibility::Collapsed);
}

bool UStoryScreen::IsTyping() const
{
	return bLineShown && TypedCharacters < FullLine.Len();
}

bool UStoryScreen::ConsumeLineAdvance()
{
	if (!bLineAdvanced)
	{
		return false;
	}
	HideLine();
	return true;
}

// 가장 긴 줄의 폭. 여러 줄 대사는 줄마다 왼쪽을 맞춘다.
float UStoryScreen::MeasureLineWidth(const FString& Text) const
{
	const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	TArray<FString> Lines;
	Text.ParseIntoArrayLines(Lines);
	float Widest = 0.f;
	for (const FString& Line : Lines)
	{
		Widest = FMath::Max(Widest, static_cast<float>(Measure->Measure(Line, LineText->GetFont()).X));
	}
	return Widest;
}

void UStoryScreen::FadeTo(const FLinearColor& Color, const float Alpha, const float Seconds)
{
	FadeFrom = FadeImage->GetColorAndOpacity().A;
	FadeColor = Color;
	FadeTarget = Alpha;
	FadeSeconds = Seconds;
	FadeElapsed = 0.f;
}

void UStoryScreen::SetEyelids(const float Openness, const float Seconds)
{
	EyelidFrom = EyelidOpenness;
	EyelidTarget = Openness;
	EyelidSeconds = Seconds;
	EyelidElapsed = 0.f;
	if (Seconds <= 0.f)
	{
		EyelidOpenness = Openness;
		ApplyEyelids();
	}
}

// 눈꺼풀 그림은 화면 반 높이보다 크다. 다 뜨면 화면 밖으로 나간다.
void UStoryScreen::ApplyEyelids()
{
	const bool bVisible = EyelidOpenness < EyelidHiddenOpenness;
	const ESlateVisibility LidVisibility = bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	EyelidTop->SetVisibility(LidVisibility);
	EyelidBottom->SetVisibility(LidVisibility);
	const float ScreenHeight = FMath::Max(Canvas->GetCachedGeometry().GetLocalSize().Y, 1080.0);
	const float Travel = ScreenHeight * EyelidTravel * EyelidOpenness;
	EyelidTop->SetRenderTranslation(FVector2D(0.f, -Travel));
	EyelidBottom->SetRenderTranslation(FVector2D(0.f, Travel));
}

void UStoryScreen::ShowTitleCard(const FName ChapterKey, const FName TitleKey, const float Seconds)
{
	ChapterText->SetText(UMenuLocalizationSubsystem::GetStoryText(ChapterKey));
	TitleText->SetText(UMenuLocalizationSubsystem::GetStoryText(TitleKey));
	TitleSeconds = Seconds;
	TitleElapsed = 0.f;
}

void UStoryScreen::BeginMash(const FName PromptKey)
{
	bMashing = true;
	MashPresses = 0;
	MashText->SetText(UMenuLocalizationSubsystem::GetStoryText(PromptKey));
	MashBar->SetPercent(0.f);
	MashBox->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UStoryScreen::SetMashProgress(const float Progress)
{
	MashBar->SetPercent(Progress);
}

int32 UStoryScreen::ConsumeMashPresses()
{
	const int32 Presses = MashPresses;
	MashPresses = 0;
	return Presses;
}

void UStoryScreen::EndMash()
{
	bMashing = false;
	MashBox->SetVisibility(ESlateVisibility::Collapsed);
}

void UStoryScreen::SetPlayerControl(const bool bControl, const FName ObjectiveKey)
{
	bPlayerControl = bControl;
	Root->SetVisibility(bControl ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Visible);
	ObjectiveText->SetVisibility(bControl ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (bControl)
	{
		ObjectiveText->SetText(UMenuLocalizationSubsystem::GetStoryText(ObjectiveKey));
		GameplayScreen = UGameplayScreen::Create(GetOwningPlayer(), OnBack);
		GetOwningLocalPlayer()->GetSubsystem<UUiSubsystem>()->PushScreen(GameplayScreen, EUiScreenLayer::Menu);
	}
	else if (GameplayScreen)
	{
		GameplayScreen->DeactivateWidget();
		GameplayScreen = nullptr;
	}
}

void UStoryScreen::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// 위젯은 일시정지 중에도 틱이 돈다. 감독의 단계와 어긋나지 않게 함께 멈춘다.
	if (GetWorld()->IsPaused())
	{
		return;
	}
	TickTyping(InDeltaTime);
	TickLetterbox(InDeltaTime);
	TickTitleCard(InDeltaTime);

	FadeElapsed += InDeltaTime;
	const float FadeAlpha = FadeSeconds > 0.f ? FMath::Clamp(FadeElapsed / FadeSeconds, 0.f, 1.f) : 1.f;
	FLinearColor Cover = FadeColor;
	Cover.A = FMath::Lerp(FadeFrom, FadeTarget, FadeAlpha);
	FadeImage->SetColorAndOpacity(Cover);

	if (EyelidElapsed < EyelidSeconds)
	{
		EyelidElapsed += InDeltaTime;
		EyelidOpenness = FMath::InterpEaseInOut(EyelidFrom, EyelidTarget, FMath::Clamp(EyelidElapsed / EyelidSeconds, 0.f, 1.f), 2.f);
		ApplyEyelids();
	}
}

void UStoryScreen::TickTyping(const float DeltaSeconds)
{
	if (!bLineShown)
	{
		return;
	}
	LineElapsed += DeltaSeconds;
	const float Appear = FMath::Clamp(LineElapsed / LineRiseSeconds, 0.f, 1.f);
	LineBox->SetRenderOpacity(UiMotion::EaseOutCubic(Appear));
	LineBox->SetRenderTranslation(FVector2D(0.f, LineRise * (1.f - UiMotion::EaseOutExpo(Appear))));

	if (!IsTyping())
	{
		// 넘김 표시는 천천히 깜빡이며 오르내린다.
		const float Wave = 0.5f + 0.5f * FMath::Sin(LineElapsed * NextMarkSpeed);
		NextMark->SetRenderOpacity(FMath::Lerp(NextMarkMinOpacity, 1.f, Wave));
		NextMark->SetRenderTranslation(FVector2D(0.f, NextMarkBob * Wave));
		return;
	}
	TypingPause -= DeltaSeconds;
	if (TypingPause > 0.f)
	{
		return;
	}
	const int32 Before = FMath::FloorToInt32(TypedCharacters);
	const float Speed = CharactersPerSecond * (LineText->GetFont().Size > LineSizeNormal ? ShoutSpeedScale : 1.f);
	TypedCharacters = FMath::Min(TypedCharacters + Speed * DeltaSeconds, static_cast<float>(FullLine.Len()));
	const int32 After = FMath::FloorToInt32(TypedCharacters);
	if (After == Before)
	{
		return;
	}
	LineText->SetText(FText::FromString(FullLine.Left(After)));
	const TCHAR Last = FullLine[After - 1];
	if (FString(TEXT(".,!?—…。、！？")).Contains(FString::Chr(Last)))
	{
		TypingPause = PunctuationPause;
	}
	else if (VoiceSound && !FChar::IsWhitespace(Last) && After - LastVoiceCharacter >= CharactersPerVoice)
	{
		LastVoiceCharacter = After;
		UGameplayStatics::PlaySound2D(this, VoiceSound, VoiceVolume, VoicePitch + FMath::FRandRange(-VoicePitchJitter, VoicePitchJitter));
	}
}

// 연출 중에는 위아래 검은 띠가 내려오고, 직접 걷는 동안에는 걷힌다.
void UStoryScreen::TickLetterbox(const float DeltaSeconds)
{
	LetterboxAmount = FMath::FInterpTo(LetterboxAmount, bPlayerControl ? 0.f : 1.f, DeltaSeconds, LetterboxSpeed);
	LetterboxTop->SetRenderTranslation(FVector2D(0.f, -LetterboxHeight * (1.f - LetterboxAmount)));
	LetterboxBottom->SetRenderTranslation(FVector2D(0.f, LetterboxHeight * (1.f - LetterboxAmount)));
}

void UStoryScreen::TickTitleCard(const float DeltaSeconds)
{
	if (TitleElapsed >= TitleSeconds)
	{
		TitleBox->SetRenderOpacity(0.f);
		return;
	}
	TitleElapsed += DeltaSeconds;
	const float Opacity = FMath::Min(TitleElapsed / TitleFadeSeconds, (TitleSeconds - TitleElapsed) / TitleFadeSeconds);
	TitleBox->SetRenderOpacity(FMath::Clamp(Opacity, 0.f, 1.f));
}
