#include "UI/Title/TitleWidget.h"

#include "UI/Common/ButtonClickBinding.h"
#include "UI/Common/EditorTransactionReset.h"
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
#include "UI/Core/UiScreen.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TitleWidget)

DEFINE_LOG_CATEGORY_STATIC(LogTitleWidget, Log, All);

namespace
{
	const FName LunaChatHintKey(TEXT("Title.LunaChatHint"));

	// Where the tail sits across the bubble's width. The tail tip stays on Luna's head and the body grows mostly to the
	// left, so even a full-width answer (wrap 420 + padding) ends before the Game Settings button at the top right.
	constexpr float LunaSpeechTailFraction = 0.75f;

	void TravelTitleToListenMap(const UObject* WorldContextObject, const FString& MapName)
	{
		if (UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr)
		{
			if (World->GetNetMode() != NM_Client && World->GetNetDriver())
			{
				World->ServerTravel(FString::Printf(TEXT("%s?listen"), *MapName));
				return;
			}
		}

		UGameplayStatics::OpenLevel(WorldContextObject, FName(*MapName), true, TEXT("listen"));
	}
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

	// The bubble shares the portrait's anchors, so the portrait's layout maps the head point directly.
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
		LunaChatInput->SetHintText(MenuTextOrFallback(LunaChatHintKey,
			NSLOCTEXT("TitleWidget", "LunaChatHint", "Ask Luna anything (Enter)")));
	}
}

void UTitleWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// Built before NativeConstruct so the localized font pass also covers the bubble text.
	BuildLunaSpeechBubble();
	BuildLunaChatInput();

	// Platforms that cannot open a browser do not show the website button.
	if (Btn_Website && !FPlatformProcess::CanLaunchURL(*OfficialWebsiteUrl))
	{
		Btn_Website->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UTitleWidget::SetBossRaidEnabled(const bool bEnabled) const
{
	if (Btn_BossRaid)
	{
		Btn_BossRaid->SetIsEnabled(bEnabled);
	}
}

void UTitleWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);

	ApplyWidgetDefinitionSettings();

	LoadLocalProfile();

	AudioVolumeControl = NewObject<UAudioVolumeControl>(this);
	AudioVolumeControl->Initialize(this, AudioVolumeSlider_, Btn_Sound);

	PdButtonClick::Bind(this, GetMenuButtonBindings());

	if (UOnlineSessionsSubsystem* OnlineSessionsSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UOnlineSessionsSubsystem>()
		: nullptr)
	{
		if (!QuickMatchRequestCompleteHandle.IsValid())
		{
			QuickMatchRequestCompleteHandle =
				OnlineSessionsSubsystem->OnQuickMatchRequestComplete.AddUObject(
					this,
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

	PdButtonClick::Unbind(this, GetMenuButtonBindings());

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
		UnbindRecordCloseButton();
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
			GuideWidgetClass = TSubclassOf<UUserWidget>(ResolvedGuideWidgetClass.Get());
		}
		if (const TSubclassOf<URecordWidget> ResolvedRecordWidgetClass = WidgetDefinition->GetRecordWidgetClass())
		{
			RecordWidgetClass = TSubclassOf<UUserWidget>(ResolvedRecordWidgetClass.Get());
		}
		QuickMatchMaxSearchResults = FMath::Max(Settings.QuickMatchMaxSearchResults, 1);
		QuickMatchMaxPublicConnections = FMath::Max(Settings.QuickMatchMaxPublicConnections, 1);
		QuickMatchRoomName = Settings.QuickMatchRoomName;
		bQuickMatchLAN = Settings.bQuickMatchLAN;
		bQuickMatchUseLobbies = Settings.bQuickMatchUseLobbies;
	}
}

void UTitleWidget::BuildLunaSpeechBubble()
{
	// Same canvas as Luna's portrait, one layer above it, so the bubble follows the title's responsive scale.
	UCanvasPanel* Canvas = Img_TitleCharacter ? Cast<UCanvasPanel>(Img_TitleCharacter->GetParent()) : nullptr;
	const UCanvasPanelSlot* PortraitSlot = Img_TitleCharacter ? Cast<UCanvasPanelSlot>(Img_TitleCharacter->Slot) : nullptr;
	if (!Canvas || !PortraitSlot || !WidgetTree || LunaSpeechBubble)
	{
		return;
	}

	// Colours follow the title menu: panel outline and hint text.
	FSlateBrush TailBrush;
	TailBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	TailBrush.TintColor = FSlateColor(FLinearColor(0.955f, 0.896f, 0.776f));
	TailBrush.OutlineSettings.Color = FSlateColor(FLinearColor(0.701f, 0.474f, 0.195f));
	TailBrush.OutlineSettings.Width = 2.f;
	TailBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
	FSlateBrush BodyBrush = TailBrush;
	BodyBrush.OutlineSettings.CornerRadii = FVector4(14.f, 14.f, 14.f, 14.f);

	UOverlay* Bubble = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("LunaSpeechBubble"));

	// A rotated square behind the body; only its lower half shows, as the tail pointing at Luna.
	// The spacers on both sides keep it at LunaSpeechTailFraction of the width, whatever the text length.
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
	BubbleSlot->SetAlignment(FVector2D(LunaSpeechTailFraction, 1.f)); // The tail tip is placed on the head point.
	BubbleSlot->SetZOrder(PortraitSlot->GetZOrder() + 1);

	Bubble->SetVisibility(ESlateVisibility::Collapsed);
	LunaSpeechBubble = Bubble;
}

void UTitleWidget::BuildLunaChatInput()
{
	// Same canvas and anchors as the portrait, at its lower edge, so the box stays under Luna at every resolution.
	UCanvasPanel* Canvas = Img_TitleCharacter ? Cast<UCanvasPanel>(Img_TitleCharacter->GetParent()) : nullptr;
	const UCanvasPanelSlot* PortraitSlot = Img_TitleCharacter ? Cast<UCanvasPanelSlot>(Img_TitleCharacter->Slot) : nullptr;
	if (!Canvas || !PortraitSlot || !WidgetTree || LunaChatInput)
	{
		return;
	}

	LunaChatInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("Input_LunaChat"));

	// UE 5.8's SetWidgetStyle hands Slate the address of its argument once the Slate widget exists.
	// The Slate widget is not built yet here, so the style is copied into the UMG member only.
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

	// The player may type in any language, so the CJK font (which also covers Latin) is used regardless of the menu language.
	FEditableTextBoxStyle Style = LunaChatInput->GetWidgetStyle();
	FSlateFontInfo Font = Style.TextStyle.Font;
	const UGameInstance* GameInstance = GetGameInstance();
	if (const UMenuLocalizationSubsystem* Localization = GameInstance ? GameInstance->GetSubsystem<UMenuLocalizationSubsystem>() : nullptr)
	{
		if (UFont* CjkFont = Localization->GetFontForLanguage(EGuideLanguage::Korean))
		{
			Font.FontObject = CjkFont;
			Font.TypefaceFontName = NAME_None;
		}
	}
	Font.Size = 16;
	const FSlateColor TextColor(FLinearColor(0.130f, 0.056f, 0.021f));
	Style.SetBackgroundImageNormal(Background)
		.SetBackgroundImageHovered(Background)
		.SetBackgroundImageFocused(FocusedBackground)
		.SetBackgroundImageReadOnly(Background)
		.SetPadding(FMargin(16.f, 10.f))
		.SetFont(Font)
		.SetForegroundColor(TextColor)
		.SetFocusedForegroundColor(TextColor);
	LunaChatInput->SetWidgetStyle(Style);
	LunaChatInput->SetHintText(MenuTextOrFallback(LunaChatHintKey,
		NSLOCTEXT("TitleWidget", "LunaChatHint", "Ask Luna anything (Enter)")));
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

	UnbindRecordCloseButton();
	if (UCommonActivatableWidget* Screen = UCommonUIActionRouterBase::FindOwningActivatable(RecordWidget->GetCachedWidget(), GetOwningLocalPlayer()))
		Screen->DeactivateWidget();
	RecordWidget->RemoveFromParent();
}

void UTitleWidget::HandleExitClicked()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, true);
}

FString UTitleWidget::GetResolvedLobbyTravelMapName() const
{
	const ULevelDefinition* Definition =
		ULevelDefinition::ResolveDefaultDefinition();
	return Definition ? Definition->GetLobbyTravelMapName() : FString();
}

FString UTitleWidget::GetResolvedRoomTravelMapName() const
{
	const ULevelDefinition* Definition =
		ULevelDefinition::ResolveDefaultDefinition();
	return Definition ? Definition->GetRoomTravelMapName() : FString();
}

FString UTitleWidget::GetResolvedTrainingRoomTravelMapName() const
{
	const ULevelDefinition* Definition =
		ULevelDefinition::ResolveDefaultDefinition();
	return Definition
		? Definition->GetTrainingRoomTravelMapName()
		: FString();
}

void UTitleWidget::OpenRoomList()
{
	const FString RoomMapName = GetResolvedRoomTravelMapName();
	if (RoomMapName.IsEmpty())
	{
		return;
	}

	PdEditorTransaction::ResetIfContainsPieObjects();
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
		if (ULobbyRuntimeSubsystem* LobbyRuntimeSubsystem =
			GameInstance->GetSubsystem<ULobbyRuntimeSubsystem>())
		{
			LobbyRuntimeSubsystem->BeginGameEntryContentPreload();
		}
	}
	PdEditorTransaction::ResetIfContainsPieObjects();
	UGameplayStatics::OpenLevel(this, FName(*TrainingRoomMapName));
}

void UTitleWidget::OpenShop()
{
	LoadLocalProfile();
	PdEditorTransaction::ResetIfContainsPieObjects();

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
	PdEditorTransaction::ResetIfContainsPieObjects();

	APlayerController* PlayerController = GetOwningPlayer();
	if (!PlayerController)
	{
		return;
	}

	const TSubclassOf<UUserWidget> ResolvedGuideWidgetClass = ResolveGuideWidgetClass();
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

		GuideWidget = CreateWidget<UUserWidget>(PlayerController, ResolvedGuideWidgetClass);
	}

	if (!GuideWidget)
	{
		return;
	}

	if (UGuideWidget* TypedGuideWidget = Cast<UGuideWidget>(GuideWidget))
	{
		TypedGuideWidget->RefreshGuide();
	}

	PresentMenuPopup(GuideWidget, FSimpleDelegate::CreateWeakLambda(this, [this]()
	{
		if (UGuideWidget* Guide = Cast<UGuideWidget>(GuideWidget)) Guide->CloseGuide();
	}));
}

void UTitleWidget::OpenRecord()
{
	PdEditorTransaction::ResetIfContainsPieObjects();

	APlayerController* PlayerController = GetOwningPlayer();
	if (!PlayerController)
	{
		return;
	}

	const TSubclassOf<UUserWidget> ResolvedRecordWidgetClass = ResolveRecordWidgetClass();
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
			UnbindRecordCloseButton();
			DismissMenuPopup(RecordWidget);
			RecordWidget = nullptr;
		}

		RecordWidget = CreateWidget<UUserWidget>(PlayerController, ResolvedRecordWidgetClass);
	}

	if (!RecordWidget)
	{
		return;
	}

	PresentMenuPopup(RecordWidget, FSimpleDelegate::CreateUObject(this, &ThisClass::HandleRecordCloseClicked));

	if (URecordWidget* TypedRecordWidget = Cast<URecordWidget>(RecordWidget))
	{
		TypedRecordWidget->RefreshRecords();
	}
	else
	{
		BindRecordCloseButton();
	}
}

void UTitleWidget::BindRecordCloseButton()
{
	UButton* RecordCloseButton = FindRecordCloseButton();
	if (!RecordCloseButton)
	{
		return;
	}

	RecordCloseButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleRecordCloseClicked);
	RecordCloseButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRecordCloseClicked);
}

void UTitleWidget::UnbindRecordCloseButton()
{
	if (UButton* RecordCloseButton = FindRecordCloseButton())
	{
		RecordCloseButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleRecordCloseClicked);
	}
}

UButton* UTitleWidget::FindRecordCloseButton() const
{
	return IsValid(RecordWidget) ? Cast<UButton>(RecordWidget->GetWidgetFromName(TEXT("Btn_Close"))) : nullptr;
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

TSubclassOf<UUserWidget> UTitleWidget::ResolveGuideWidgetClass() const
{
	if (GuideWidgetClass)
	{
		return GuideWidgetClass;
	}

	if (const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this))
	{
		return TSubclassOf<UUserWidget>(WidgetDefinition->GetGuideWidgetClass().Get());
	}

	return nullptr;
}

TSubclassOf<UUserWidget> UTitleWidget::ResolveRecordWidgetClass() const
{
	if (RecordWidgetClass)
	{
		return RecordWidgetClass;
	}

	if (const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this))
	{
		return TSubclassOf<UUserWidget>(WidgetDefinition->GetRecordWidgetClass().Get());
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
	if (ActiveQuickMatchRequestId != 0) return;
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
		QuickMatchRequestCompleteHandle =
			OnlineSessionsSubsystem->OnQuickMatchRequestComplete.AddUObject(
				this,
				&ThisClass::HandleQuickMatchRequestComplete);
	}

	const uint64 RequestId = OnlineSessionsSubsystem->BeginQuickMatch(
		GetOwningLocalPlayer(),
		QuickMatchMaxSearchResults,
		QuickMatchMaxPublicConnections,
		QuickMatchRoomName,
		TEXT("Lobby"),
		bQuickMatchLAN,
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

	PdEditorTransaction::ResetIfContainsPieObjects();
	TravelTitleToListenMap(this, LobbyMapName);
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
		OnlineSessionsSubsystem->OnQuickMatchRequestComplete.Remove(
			QuickMatchRequestCompleteHandle);
		QuickMatchRequestCompleteHandle.Reset();
	}

	const uint64 RequestId = ActiveQuickMatchRequestId;
	ActiveQuickMatchRequestId = 0;
	if (RequestId != 0)
	{
		OnlineSessionsSubsystem->CancelSessionRequest(RequestId);
	}
}

void UTitleWidget::PresentMenuPopup(UUserWidget* Popup, FSimpleDelegate OnBack)
{
	Popup->SetVisibility(ESlateVisibility::Visible);
	const UCommonActivatableWidget* ExistingScreen = UCommonUIActionRouterBase::FindOwningActivatable(Popup->GetCachedWidget(), GetOwningLocalPlayer());
	if (ExistingScreen && ExistingScreen->IsActivated())
	{
		return;
	}

	GetUiSubsystem()->PushScreen(UUiScreen::CreateBlocking(GetOwningPlayer(), Popup, Popup, MoveTemp(OnBack)), EUiScreenLayer::Menu);
}

void UTitleWidget::DismissMenuPopup(UUserWidget* Popup) const
{
	if (UCommonActivatableWidget* Screen = UCommonUIActionRouterBase::FindOwningActivatable(Popup->GetCachedWidget(), GetOwningLocalPlayer()))
	{
		Screen->DeactivateWidget();
	}
	Popup->RemoveFromParent();
}

UUiSubsystem* UTitleWidget::GetUiSubsystem() const
{
	const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	return LocalPlayer ? LocalPlayer->GetSubsystem<UUiSubsystem>() : nullptr;
}

void UTitleWidget::HandleQuickMatchRequestComplete(
	const uint64 RequestId,
	const bool bWasSuccessful,
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
