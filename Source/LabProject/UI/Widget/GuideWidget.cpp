#include "UI/Widget/GuideWidget.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Data/ContentDataSubsystem.h"
#include "Engine/Font.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"
#include "Definition/Mode/PdGameInstanceDefinition.h"
#include "Mode/PdHUD.h"
#include "Settings/BgmSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GuideWidget)

namespace
{
	constexpr int32 MaxGuideButtonBindings = 12;
	constexpr const TCHAR* GuideLanguagePlaceholder = TEXT("Select Language");

	bool TryResolveGuideLanguageOption(const FString& Option, EGuideLanguage& OutLanguage)
	{
		FString NormalizedOption = Option.TrimStartAndEnd().ToLower();
		NormalizedOption.ReplaceInline(TEXT(" "), TEXT(""));
		NormalizedOption.ReplaceInline(TEXT("_"), TEXT("-"));

		if (NormalizedOption == TEXT("russian")
			|| NormalizedOption == TEXT("russian(ru)")
			|| NormalizedOption == TEXT("ru"))
		{
			OutLanguage = EGuideLanguage::Russian;
			return true;
		}

		if (NormalizedOption == TEXT("spanish-spain")
			|| NormalizedOption == TEXT("spanish-spain(es-es)")
			|| NormalizedOption == TEXT("es-es"))
		{
			OutLanguage = EGuideLanguage::Spanish;
			return true;
		}

		if (NormalizedOption == TEXT("portuguese-brazil")
			|| NormalizedOption == TEXT("portuguese-brazil(pt-br)")
			|| NormalizedOption == TEXT("pt-br"))
		{
			OutLanguage = EGuideLanguage::PortugueseBrazil;
			return true;
		}

		if (NormalizedOption == TEXT("german")
			|| NormalizedOption == TEXT("german(de)")
			|| NormalizedOption == TEXT("de"))
		{
			OutLanguage = EGuideLanguage::German;
			return true;
		}

		if (NormalizedOption == TEXT("french")
			|| NormalizedOption == TEXT("french(fr)")
			|| NormalizedOption == TEXT("fr"))
		{
			OutLanguage = EGuideLanguage::French;
			return true;
		}

		if (NormalizedOption == TEXT("polish")
			|| NormalizedOption == TEXT("polish(pl)")
			|| NormalizedOption == TEXT("pl"))
		{
			OutLanguage = EGuideLanguage::Polish;
			return true;
		}

		if (NormalizedOption == TEXT("traditionalchinese")
			|| NormalizedOption == TEXT("traditionalchinese(zh-hant)")
			|| NormalizedOption == TEXT("zh-tw")
			|| NormalizedOption == TEXT("zh-hant"))
		{
			OutLanguage = EGuideLanguage::TraditionalChinese;
			return true;
		}

		if (NormalizedOption == TEXT("turkish")
			|| NormalizedOption == TEXT("turkish(tr)")
			|| NormalizedOption == TEXT("tr"))
		{
			OutLanguage = EGuideLanguage::Turkish;
			return true;
		}

		if (NormalizedOption == TEXT("korean")
			|| NormalizedOption == TEXT("korean(ko)")
			|| NormalizedOption == TEXT("ko")
			|| NormalizedOption == TEXT("한국어"))
		{
			OutLanguage = EGuideLanguage::Korean;
			return true;
		}

		if (NormalizedOption == TEXT("english")
			|| NormalizedOption == TEXT("english(en)")
			|| NormalizedOption == TEXT("en"))
		{
			OutLanguage = EGuideLanguage::English;
			return true;
		}

		if (NormalizedOption == TEXT("japanese")
			|| NormalizedOption == TEXT("japanese(ja)")
			|| NormalizedOption == TEXT("ja")
			|| NormalizedOption == TEXT("日本語"))
		{
			OutLanguage = EGuideLanguage::Japanese;
			return true;
		}

		if (NormalizedOption == TEXT("chinese")
			|| NormalizedOption == TEXT("simplifiedchinese")
			|| NormalizedOption == TEXT("simplifiedchinese(zh-hans)")
			|| NormalizedOption == TEXT("zh")
			|| NormalizedOption == TEXT("zh-cn")
			|| NormalizedOption == TEXT("zh-hans")
			|| NormalizedOption == TEXT("简体中文"))
		{
			OutLanguage = EGuideLanguage::SimplifiedChinese;
			return true;
		}

		if (NormalizedOption == TEXT("spanish")
			|| NormalizedOption == TEXT("spanish(es)")
			|| NormalizedOption == TEXT("es")
			|| NormalizedOption == TEXT("español"))
		{
			OutLanguage = EGuideLanguage::Spanish;
			return true;
		}

		return false;
	}

	bool HasGuideText(const FText& Text)
	{
		return !Text.IsEmptyOrWhitespace();
	}

	const TCHAR* GetGuideLanguageOptionName(const EGuideLanguage Language)
	{
		switch (Language)
		{
		case EGuideLanguage::English: return TEXT("English");
		case EGuideLanguage::SimplifiedChinese: return TEXT("Simplified Chinese");
		case EGuideLanguage::Russian: return TEXT("Russian");
		case EGuideLanguage::Spanish: return TEXT("Spanish - Spain");
		case EGuideLanguage::PortugueseBrazil: return TEXT("Portuguese - Brazil");
		case EGuideLanguage::German: return TEXT("German");
		case EGuideLanguage::Japanese: return TEXT("Japanese");
		case EGuideLanguage::French: return TEXT("French");
		case EGuideLanguage::Polish: return TEXT("Polish");
		case EGuideLanguage::Korean: return TEXT("Korean");
		case EGuideLanguage::TraditionalChinese: return TEXT("Traditional Chinese");
		case EGuideLanguage::Turkish: return TEXT("Turkish");
		default: return nullptr;
		}
	}

	FText MakeGuideText(const TCHAR* Text)
	{
		return FText::FromString(Text);
	}

	FGuidePageEntry MakeGuidePage(const FName ButtonWidgetName, const FText& Content)
	{
		FGuidePageEntry Page;
		Page.ButtonWidgetName = ButtonWidgetName;
		Page.Content = Content;
		return Page;
	}

}

UGuideWidget::UGuideWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);

	GuideButtonWidgetNames = {
		TEXT("Btn_QuickStart"),
		TEXT("Btn_ProfileAndStatus"),
		TEXT("Btn_Controls"),
		TEXT("Btn_GameRules"),
		TEXT("Btn_Modes"),
		TEXT("Btn_Pandora"),
		TEXT("Btn_ItemsAndSkins"),
		TEXT("Btn_Paint")
	};
}

void UGuideWidget::NativeConstruct()
{
	Super::NativeConstruct();
	bIsClosing = false;
	SetIsFocusable(true);
	ResolveWidgets();
	InitializeLanguageOptions();
	ApplyBackgroundPatternVisibility();
	if (Btn_Close)
	{
		Btn_Close->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCloseClicked);
	}

	if (CB_Language)
	{
		CB_Language->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::HandleLanguageSelectionChanged);
	}

	if (UBgmSubsystem* BgmSubsystem = UGameInstance::GetSubsystem<UBgmSubsystem>(GetGameInstance()))
	{
		BgmSubsystem->PlayBgmForContext(EBgmContext::Guide);
	}

	BeginContentPreload();
	RefreshGuide();
}

void UGuideWidget::NativeDestruct()
{
	ReleaseContentPreloads();
	UnbindGuideButtons();

	if (Btn_Close)
	{
		Btn_Close->OnClicked.RemoveDynamic(this, &ThisClass::HandleCloseClicked);
	}

	if (CB_Language)
	{
		CB_Language->OnSelectionChanged.RemoveDynamic(this, &ThisClass::HandleLanguageSelectionChanged);
	}

	if (UBgmSubsystem* BgmSubsystem = UGameInstance::GetSubsystem<UBgmSubsystem>(GetGameInstance()))
	{
		BgmSubsystem->RestoreWorldBgm();
	}

	Super::NativeDestruct();
}

void UGuideWidget::BeginContentPreload()
{
	ReleaseContentPreloads();
	const int32 PreloadGeneration = ++ContentPreloadGeneration;

	const UGameInstance* GameInstance = GetGameInstance();
	UContentDataSubsystem* ContentSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
	const TSoftObjectPtr<UGuideDefinition>& GuideDefinition =
		UPdGameInstanceDefinition::GetConfiguredDefinitionReferences().Guide;
	if (!ContentSubsystem || GuideDefinition.IsNull())
	{
		return;
	}

	GuideDefinitionPreloadHandle =
		ContentSubsystem->PreloadSoftObjectPathsAsync(
			{GuideDefinition.ToSoftObjectPath()},
			FSimpleDelegate::CreateWeakLambda(
				this,
				[this, PreloadGeneration]()
				{
					BeginPageImagePreload(PreloadGeneration);
				}));
}

void UGuideWidget::BeginPageImagePreload(const int32 PreloadGeneration)
{
	if (PreloadGeneration != ContentPreloadGeneration)
	{
		return;
	}

	const UGuideDefinition* LoadedGuideData = ResolveGuideDefinition();
	const UGameInstance* GameInstance = GetGameInstance();
	UContentDataSubsystem* ContentSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
	if (!LoadedGuideData || !ContentSubsystem)
	{
		RefreshGuide();
		return;
	}

	InitializeLanguageOptions();

	TArray<FSoftObjectPath> ImagePaths;
	for (const FGuidePageEntry& Page : LoadedGuideData->Pages)
	{
		ImagePaths.Add(Page.Image.ToSoftObjectPath());
	}
	ImagePaths.Add(LoadedGuideData->PolishContentFont.ToSoftObjectPath());
	ImagePaths.Add(LoadedGuideData->TurkishContentFont.ToSoftObjectPath());

	GuideImagePreloadHandle =
		ContentSubsystem->PreloadSoftObjectPathsAsync(
			ImagePaths,
			FSimpleDelegate::CreateWeakLambda(
				this,
				[this, PreloadGeneration]()
				{
					if (PreloadGeneration == ContentPreloadGeneration)
					{
						RefreshGuide();
					}
				}));
}

void UGuideWidget::ReleaseContentPreloads()
{
	++ContentPreloadGeneration;

	auto ReleaseHandle = [](TSharedPtr<FStreamableHandle>& Handle)
	{
		if (Handle.IsValid())
		{
			Handle->CancelHandle();
			Handle->ReleaseHandle();
			Handle.Reset();
		}
	};

	ReleaseHandle(GuideImagePreloadHandle);
	ReleaseHandle(GuideDefinitionPreloadHandle);
}

const UGuideDefinition* UGuideWidget::ResolveGuideDefinition() const
{
	return UPdGameInstanceDefinition::GetConfiguredDefinitionReferences().Guide.Get();
}

void UGuideWidget::RefreshGuide()
{
	ResolveWidgets();
	RebuildPages();
	ResolveSelectedLanguage();
	BindGuideButtons();

	if (CachedPages.IsEmpty())
	{
		if (Txt_Content)
		{
			Txt_Content->SetText(FText::GetEmpty());
		}
		ApplyImage(nullptr);
		return;
	}

	SelectGuidePage(CachedPages.IsValidIndex(CurrentPageIndex) ? CurrentPageIndex : 0);
}

void UGuideWidget::SelectGuidePage(const int32 PageIndex)
{
	if (!CachedPages.IsValidIndex(PageIndex))
	{

		return;
	}

	CurrentPageIndex = PageIndex;
	ApplyPage(CachedPages[PageIndex]);
	SelectBoundGuideButton(PageIndex);
}

void UGuideWidget::CloseGuide()
{
	if (bIsClosing)
	{
		return;
	}

	bIsClosing = true;
	RemoveFromParent();
	OnGuideClosed.Broadcast(this);
}

void UGuideWidget::SetOpenedFromGameplayMenu(const bool bInOpenedFromGameplayMenu)
{
	bOpenedFromGameplayMenu = bInOpenedFromGameplayMenu;
	ResolveWidgets();
	ApplyBackgroundPatternVisibility();
}

FReply UGuideWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		if (APlayerController* PlayerController = GetOwningPlayer())
		{
			if (APdHUD* Hud = PlayerController->GetHUD<APdHUD>())
			{
				Hud->HandleEscapeInput();
				return FReply::Handled();
			}
		}

		CloseGuide();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UGuideWidget::HandleCloseClicked()
{
	CloseGuide();
}

void UGuideWidget::HandleLanguageSelectionChanged(
	const FString SelectedItem,
	const ESelectInfo::Type SelectionType)
{
	EGuideLanguage SelectedLanguage = EGuideLanguage::Korean;
	bHasSelectedLanguage = TryResolveGuideLanguageOption(SelectedItem, SelectedLanguage);
	if (bHasSelectedLanguage)
	{
		CurrentLanguage = SelectedLanguage;
	}

	if (CachedPages.IsValidIndex(CurrentPageIndex))
	{
		ApplyPage(CachedPages[CurrentPageIndex]);
	}
}

void UGuideWidget::HandleGuideButton0Clicked() { SelectGuidePage(0); }
void UGuideWidget::HandleGuideButton1Clicked() { SelectGuidePage(1); }
void UGuideWidget::HandleGuideButton2Clicked() { SelectGuidePage(2); }
void UGuideWidget::HandleGuideButton3Clicked() { SelectGuidePage(3); }
void UGuideWidget::HandleGuideButton4Clicked() { SelectGuidePage(4); }
void UGuideWidget::HandleGuideButton5Clicked() { SelectGuidePage(5); }
void UGuideWidget::HandleGuideButton6Clicked() { SelectGuidePage(6); }
void UGuideWidget::HandleGuideButton7Clicked() { SelectGuidePage(7); }
void UGuideWidget::HandleGuideButton8Clicked() { SelectGuidePage(8); }
void UGuideWidget::HandleGuideButton9Clicked() { SelectGuidePage(9); }
void UGuideWidget::HandleGuideButton10Clicked() { SelectGuidePage(10); }
void UGuideWidget::HandleGuideButton11Clicked() { SelectGuidePage(11); }

void UGuideWidget::ResolveWidgets()
{
	if (!Txt_Content)
	{
		Txt_Content = Cast<UTextBlock>(GetWidgetFromName(TEXT("Txt_Content")));
	}
	if (Txt_Content && !bCapturedDefaultContentFont)
	{
		DefaultContentFont = Txt_Content->GetFont();
		bCapturedDefaultContentFont = true;
	}

	if (!ImageBorder)
	{
		ImageBorder = GetWidgetFromName(TEXT("ImageBorder"));
	}

	if (!Btn_Close)
	{
		Btn_Close = Cast<UButton>(GetWidgetFromName(TEXT("Btn_Close")));
	}

	if (!CB_Language)
	{
		CB_Language = Cast<UComboBoxString>(GetWidgetFromName(TEXT("CB_Language")));
	}

	if (!Img_BackgroundPattern)
	{
		Img_BackgroundPattern = GetWidgetFromName(TEXT("Img_BackgroundPattern"));
	}
	if (Img_BackgroundPattern && !bCapturedBackgroundPatternVisibility)
	{
		DefaultBackgroundPatternVisibility = Img_BackgroundPattern->GetVisibility();
		bCapturedBackgroundPatternVisibility = true;
	}
}

void UGuideWidget::InitializeLanguageOptions()
{
	if (!CB_Language)
	{
		return;
	}

	EGuideLanguage SelectedLanguage = CurrentLanguage;
	const bool bShouldPreserveSelectedLanguage = bHasSelectedLanguage
		&& TryResolveGuideLanguageOption(CB_Language->GetSelectedOption(), SelectedLanguage);

	TArray<EGuideLanguage> LanguageOrder = {
		EGuideLanguage::English,
		EGuideLanguage::SimplifiedChinese,
		EGuideLanguage::Russian,
		EGuideLanguage::Spanish,
		EGuideLanguage::PortugueseBrazil,
		EGuideLanguage::German,
		EGuideLanguage::Japanese,
		EGuideLanguage::French,
		EGuideLanguage::Polish,
		EGuideLanguage::Korean,
		EGuideLanguage::TraditionalChinese,
		EGuideLanguage::Turkish
	};
	if (const UGuideDefinition* LoadedGuideData = ResolveGuideDefinition();
		LoadedGuideData && !LoadedGuideData->SupportedLanguages.IsEmpty())
	{
		LanguageOrder = LoadedGuideData->SupportedLanguages;
	}

	CB_Language->ClearOptions();
	CB_Language->AddOption(GuideLanguagePlaceholder);
	FString SelectedOption(GuideLanguagePlaceholder);
	bool bPreservedOptionAdded = false;
	for (const EGuideLanguage Language : LanguageOrder)
	{
		const TCHAR* OptionName = GetGuideLanguageOptionName(Language);
		if (!OptionName)
		{
			continue;
		}

		const FString Option(OptionName);
		CB_Language->AddOption(Option);
		if (bShouldPreserveSelectedLanguage && Language == SelectedLanguage)
		{
			SelectedOption = Option;
			bPreservedOptionAdded = true;
		}
	}

	CB_Language->SetSelectedOption(SelectedOption);
	bHasSelectedLanguage = bShouldPreserveSelectedLanguage && bPreservedOptionAdded;
	if (bHasSelectedLanguage)
	{
		CurrentLanguage = SelectedLanguage;
	}
}

void UGuideWidget::ApplyBackgroundPatternVisibility()
{
	if (!Img_BackgroundPattern)
	{
		return;
	}

	Img_BackgroundPattern->SetVisibility(
		bOpenedFromGameplayMenu
			? ESlateVisibility::Hidden
			: DefaultBackgroundPatternVisibility);
}

void UGuideWidget::RebuildPages()
{
	CachedPages.Reset();

	if (const UGuideDefinition* LoadedGuideData = ResolveGuideDefinition())
	{
		CachedPages = LoadedGuideData->Pages;
	}

	if (!CachedPages.IsEmpty())
	{
		return;
	}

	CachedPages = {
		MakeGuidePage(TEXT("Btn_QuickStart"), MakeGuideText(TEXT("Choose Training Mode to test weapons, Pandora powers, items, and painting before joining a match."))),
		MakeGuidePage(TEXT("Btn_ProfileAndStatus"), MakeGuideText(TEXT("Open the Character Hub to review your profile, check status values, and manage character growth."))),
		MakeGuidePage(TEXT("Btn_Controls"), MakeGuideText(TEXT("Move, aim, attack, dodge, use skills, open the Character Hub, and chat using the configured input bindings."))),
		MakeGuidePage(TEXT("Btn_GameRules"), MakeGuideText(TEXT("Defeat enemies, score points, respawn after death, and chase the final reward before the match ends."))),
		MakeGuidePage(TEXT("Btn_Modes"), MakeGuideText(TEXT("Use Training Mode, Quick Match, Room List, Shop, and Gameplay to move through the main game flow."))),
		MakeGuidePage(TEXT("Btn_Pandora"), MakeGuideText(TEXT("Pandora powers are special blessings that change how your character fights."))),
		MakeGuidePage(TEXT("Btn_ItemsAndSkins"), MakeGuideText(TEXT("Equip weapons, gear, consumables, skins, gestures, and pets from the Character Hub and Shop."))),
		MakeGuidePage(TEXT("Btn_Paint"), MakeGuideText(TEXT("Draw on the canvas, export it above your character, or project your drawing onto the face decal.")))
	};
}

void UGuideWidget::BindGuideButtons()
{
	UnbindGuideButtons();

	BoundGuideButtons.SetNum(CachedPages.Num());
	for (int32 PageIndex = 0; PageIndex < CachedPages.Num() && PageIndex < MaxGuideButtonBindings; ++PageIndex)
	{
		UButton* Button = FindButtonForPage(PageIndex, CachedPages[PageIndex]);
		if (!Button)
		{

			continue;
		}

		BindGuideButton(PageIndex, Button);
	}
}

void UGuideWidget::UnbindGuideButtons()
{
	for (int32 PageIndex = 0; PageIndex < BoundGuideButtons.Num(); ++PageIndex)
	{
		if (UButton* Button = BoundGuideButtons[PageIndex])
		{
			UnbindGuideButton(PageIndex, Button);
		}
	}

	BoundGuideButtons.Reset();
}

void UGuideWidget::BindGuideButton(const int32 PageIndex, UButton* Button)
{
	if (!Button || PageIndex < 0 || PageIndex >= MaxGuideButtonBindings)
	{
		return;
	}

	switch (PageIndex)
	{
	case 0: Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleGuideButton0Clicked); break;
	case 1: Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleGuideButton1Clicked); break;
	case 2: Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleGuideButton2Clicked); break;
	case 3: Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleGuideButton3Clicked); break;
	case 4: Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleGuideButton4Clicked); break;
	case 5: Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleGuideButton5Clicked); break;
	case 6: Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleGuideButton6Clicked); break;
	case 7: Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleGuideButton7Clicked); break;
	case 8: Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleGuideButton8Clicked); break;
	case 9: Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleGuideButton9Clicked); break;
	case 10: Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleGuideButton10Clicked); break;
	case 11: Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleGuideButton11Clicked); break;
	default: break;
	}

	if (BoundGuideButtons.IsValidIndex(PageIndex))
	{
		BoundGuideButtons[PageIndex] = Button;
	}
}

void UGuideWidget::UnbindGuideButton(const int32 PageIndex, UButton* Button)
{
	if (!Button || PageIndex < 0 || PageIndex >= MaxGuideButtonBindings)
	{
		return;
	}

	switch (PageIndex)
	{
	case 0: Button->OnClicked.RemoveDynamic(this, &ThisClass::HandleGuideButton0Clicked); break;
	case 1: Button->OnClicked.RemoveDynamic(this, &ThisClass::HandleGuideButton1Clicked); break;
	case 2: Button->OnClicked.RemoveDynamic(this, &ThisClass::HandleGuideButton2Clicked); break;
	case 3: Button->OnClicked.RemoveDynamic(this, &ThisClass::HandleGuideButton3Clicked); break;
	case 4: Button->OnClicked.RemoveDynamic(this, &ThisClass::HandleGuideButton4Clicked); break;
	case 5: Button->OnClicked.RemoveDynamic(this, &ThisClass::HandleGuideButton5Clicked); break;
	case 6: Button->OnClicked.RemoveDynamic(this, &ThisClass::HandleGuideButton6Clicked); break;
	case 7: Button->OnClicked.RemoveDynamic(this, &ThisClass::HandleGuideButton7Clicked); break;
	case 8: Button->OnClicked.RemoveDynamic(this, &ThisClass::HandleGuideButton8Clicked); break;
	case 9: Button->OnClicked.RemoveDynamic(this, &ThisClass::HandleGuideButton9Clicked); break;
	case 10: Button->OnClicked.RemoveDynamic(this, &ThisClass::HandleGuideButton10Clicked); break;
	case 11: Button->OnClicked.RemoveDynamic(this, &ThisClass::HandleGuideButton11Clicked); break;
	default: break;
	}
}

UButton* UGuideWidget::FindButtonForPage(const int32 PageIndex, const FGuidePageEntry& Page) const
{
	if (!Page.ButtonWidgetName.IsNone())
	{
		if (UButton* Button = Cast<UButton>(GetWidgetFromName(Page.ButtonWidgetName)))
		{
			return Button;
		}
	}

	if (GuideButtonWidgetNames.IsValidIndex(PageIndex) && !GuideButtonWidgetNames[PageIndex].IsNone())
	{
		if (UButton* Button = Cast<UButton>(GetWidgetFromName(GuideButtonWidgetNames[PageIndex])))
		{
			return Button;
		}
	}

	return nullptr;
}

void UGuideWidget::ApplyPage(const FGuidePageEntry& Page)
{
	if (Txt_Content)
	{
		ApplyContentFont();
		Txt_Content->SetText(ResolvePageContent(Page));
	}

	ApplyImage(Page.Image.Get());
}

void UGuideWidget::ApplyContentFont()
{
	if (!Txt_Content || !bCapturedDefaultContentFont)
	{
		return;
	}

	FSlateFontInfo ContentFont = DefaultContentFont;
	const UGuideDefinition* LoadedGuideData = ResolveGuideDefinition();
	const UFont* LanguageFont = nullptr;
	if (bHasSelectedLanguage && LoadedGuideData)
	{
		switch (CurrentLanguage)
		{
		case EGuideLanguage::Polish:
			LanguageFont = LoadedGuideData->PolishContentFont.Get();
			break;
		case EGuideLanguage::Turkish:
			LanguageFont = LoadedGuideData->TurkishContentFont.Get();
			break;
		default:
			break;
		}
	}

	if (IsValid(LanguageFont))
	{
		ContentFont.FontObject = LanguageFont;
		ContentFont.TypefaceFontName = NAME_None;
	}

	Txt_Content->SetFont(ContentFont);
}

void UGuideWidget::ApplyImage(UTexture2D* Texture)
{
	if (!ImageBorder)
	{
		return;
	}

	if (!Texture)
	{
		ImageBorder->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	ImageBorder->SetVisibility(ESlateVisibility::Visible);
	if (UBorder* Border = Cast<UBorder>(ImageBorder))
	{
		Border->SetBrushFromTexture(Texture);
		return;
	}

	if (UImage* Image = Cast<UImage>(ImageBorder))
	{
		Image->SetBrushFromTexture(Texture, true);
	}
}

void UGuideWidget::ResolveSelectedLanguage()
{
	bHasSelectedLanguage = false;

	if (CB_Language)
	{
		EGuideLanguage SelectedLanguage = EGuideLanguage::Korean;
		if (TryResolveGuideLanguageOption(CB_Language->GetSelectedOption(), SelectedLanguage))
		{
			CurrentLanguage = SelectedLanguage;
			bHasSelectedLanguage = true;
		}
	}
}

FText UGuideWidget::ResolvePageContent(const FGuidePageEntry& Page) const
{
	if (bHasSelectedLanguage)
	{
		if (const FText* LocalizedText = Page.LocalizedContent.Find(CurrentLanguage);
			LocalizedText && HasGuideText(*LocalizedText))
		{
			return *LocalizedText;
		}
	}

	return Page.Content;
}

void UGuideWidget::SelectBoundGuideButton(const int32 PageIndex)
{
	for (int32 ButtonIndex = 0; ButtonIndex < BoundGuideButtons.Num(); ++ButtonIndex)
	{
		if (UButton* Button = BoundGuideButtons[ButtonIndex])
		{
			Button->SetIsEnabled(ButtonIndex != PageIndex);
		}
	}
}
