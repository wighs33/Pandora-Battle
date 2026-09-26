#include "UI/Widget/GuideWidget.h"
#include "Input/CommonUIActionRouterBase.h"
#include "CommonActivatableWidget.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
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
#include "Settings/MenuLocalizationSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GuideWidget)

namespace
{
	constexpr int32 MaxGuideButtonBindings = 12;
	bool HasGuideText(const FText& Text)
	{
		return !Text.IsEmptyOrWhitespace();
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
	ApplyBackgroundPatternVisibility();
	if (Btn_Close)
	{
		Btn_Close->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCloseClicked);
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

	TArray<FSoftObjectPath> ImagePaths;
	for (const FGuidePageEntry& Page : LoadedGuideData->Pages)
	{
		ImagePaths.Add(Page.Image.ToSoftObjectPath());
	}

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
	if (UCommonActivatableWidget* Screen = UCommonUIActionRouterBase::FindOwningActivatable(GetCachedWidget(), GetOwningLocalPlayer()))
		Screen->DeactivateWidget();
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
	if (UCommonUIActionRouterBase::FindOwningActivatable(GetCachedWidget(), GetOwningLocalPlayer()))
		return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
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

void UGuideWidget::OnMenuLanguageChanged()
{
	RefreshGuide();
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
	// Page identity remains the DA_Guide button name; display labels come from the shared catalog.
	static const TMap<FName, FName> PageTitles = {
		{TEXT("Btn_QuickStart"), TEXT("Guide.QuickStart")}, {TEXT("Btn_Controls"), TEXT("Guide.Controls")},
		{TEXT("Btn_GameRules"), TEXT("Guide.Rules")}, {TEXT("Btn_Modes"), TEXT("Guide.Modes")},
		{TEXT("Btn_ProfileAndStatus"), TEXT("Guide.Profile")}, {TEXT("Btn_Pandora"), TEXT("Guide.Pandora")},
		{TEXT("Btn_ItemsAndSkins"), TEXT("Guide.Items")}, {TEXT("Btn_Paint"), TEXT("Guide.Paint")}};
	if (Txt_PageTitle)
	{
		const FName* Key = PageTitles.Find(Page.ButtonWidgetName);
		Txt_PageTitle->SetText(Key ? MenuText(*Key) : FText::GetEmpty());
	}
	if (Txt_PageNumber)
		Txt_PageNumber->SetText(FText::AsNumber(CurrentPageIndex + 1));
	if (Txt_Content)
	{
		ApplyContentFont();
		Txt_Content->SetText(ResolvePageContent(Page));
	}

	ApplyImage(Page.Image.Get());
}

void UGuideWidget::ApplyContentFont()
{
	if (!Txt_Content || !bCapturedDefaultContentFont) return;
	FSlateFontInfo Font = DefaultContentFont;
	if (const UMenuLocalizationSubsystem* Localization = GetLocalization())
	{
		Font.FontObject = Localization->GetFontForLanguage(CurrentLanguage);
		Font.TypefaceFontName = NAME_None;
	}
	Txt_Content->SetFont(Font);
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
	// The style supplies a ScaleBox around this size box, preserving every original
	// DA_Guide illustration's aspect ratio instead of stretching it to the panel.
	if (USizeBox* ImageSize = Cast<USizeBox>(ImageBorder->GetParent()))
	{
		ImageSize->SetWidthOverride(Texture->GetSizeX());
		ImageSize->SetHeightOverride(Texture->GetSizeY());
	}
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
	if (const UMenuLocalizationSubsystem* Localization = GetLocalization())
		CurrentLanguage = Localization->GetLanguage();
	else if (const UGuideDefinition* Definition = ResolveGuideDefinition())
		CurrentLanguage = Definition->DefaultLanguage;
}

FText UGuideWidget::ResolvePageContent(const FGuidePageEntry& Page) const
{
	if (const FText* Translation = Page.LocalizedContent.Find(CurrentLanguage); Translation && HasGuideText(*Translation))
		return *Translation;
	if (const FText* English = Page.LocalizedContent.Find(EGuideLanguage::English); English && HasGuideText(*English))
		return *English;
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
