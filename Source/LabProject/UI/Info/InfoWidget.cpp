#include "UI/Info/InfoWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetTree.h"
#include "Containers/Ticker.h"
#include "Definition/Common/ProjectTagDefinition.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "UI/Pandora/PandoraTreeWidget.h"
#include "Components/Overlay.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Item/ItemInstance.h"
#include "Engine/GameInstance.h"
#include "Profile/PlayerProfileSubsystem.h"
#include "Mode/PdHUD.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "Definition/Skin/SkinDefinition.h"
#include "Definition/UI/WidgetClassDefinition.h"
#include "UI/Info/Preview/InfoCharacterPreview.h"
#include "UI/Info/InfoDetailPopup.h"
#include "UI/Info/Map/InfoMapPanel.h"
#include "UI/Info/Paint/InfoPaintCanvas.h"
#include "UI/Info/Item/ItemSlotDragDropOperation.h"
#include "UI/Info/Item/LeftEquipmentWidget.h"
#include "UI/Info/Pandora/LeftPandoraWidget.h"
#include "UI/Info/Status/LeftProfileWidget.h"
#include "UI/Info/Skin/LeftSkinWidget.h"
#include "UI/Info/Map/MapWidget.h"
#include "UI/Pandora/PandoraDescriptionWidget.h"
#include "UI/Info/Paint/PaintCanvasWidget.h"
#include "UI/Info/Item/RightInventoryWidget.h"
#include "UI/Info/Pandora/RightPandoraWidget.h"
#include "UI/Info/Skin/RightSkinWidget.h"
#include "UI/Info/Status/RightStatusWidget.h"
#include "UI/Info/Skin/SkinSlotDragDropOperation.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InfoWidget)

UInfoWidget::UInfoWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UInfoWidget::EnsureInfoViews()
{
	if (!DetailPopup)
	{
		DetailPopup = NewObject<UInfoDetailPopup>(this);
	}
	if (!CharacterPreview)
	{
		CharacterPreview = NewObject<UInfoCharacterPreview>(this);
	}
	if (!MapPanel)
	{
		MapPanel = NewObject<UInfoMapPanel>(this);
	}
	if (!PaintCanvas)
	{
		PaintCanvas = NewObject<UInfoPaintCanvas>(this);
	}
}

void UInfoWidget::ConfigureInfoViews()
{
	if (DetailPopup)
	{
		DetailPopup->Initialize(
			this,
			WB_LeftEquipment,
			ItemDetailWidgetClass,
			PandoraDescriptionWidgetClass,
			DetailPopupOffset);
	}
	if (CharacterPreview)
	{
		CharacterPreview->Initialize(
			this,
			bUseCharacterPreviewCamera,
			CharacterPreviewClass);
	}
	if (MapPanel)
	{
		MapPanel->Initialize(
			this,
			WidgetTree,
			MapButton,
			MapOverlay,
			TotalMap,
			TotalMapWidgetClass,
			SlideMap,
			MapSlideStartOffset,
			MapSlideDuration);
	}
	if (PaintCanvas)
	{
		PaintCanvas->Initialize(this, Canvas);
	}
}

void UInfoWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	ApplyWidgetDefinitionSettings();
}

void UInfoWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ApplyWidgetDefinitionSettings();
	EnsureInfoViews();
	ConfigureInfoViews();
	SetIsFocusable(true);

	SetPaintCanvasWidgetVisible(false);

	if (ProfileTabButton)
	{
		ProfileTabButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnProfileTabButtonClicked);
	}

	if (ItemTabButton)
	{
		ItemTabButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnItemTabButtonClicked);
	}

	if (SkinButton)
	{
		SkinButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnSkinButtonClicked);
	}

	if (PandoraButton)
	{
		PandoraButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnPandoraButtonClicked);
	}

	if (MapButton)
	{
		MapButton->OnClicked.AddUniqueDynamic(this, &ThisClass::OnMapButtonClicked);
	}

	if (Btn_Setting)
	{
		Btn_Setting->OnClicked.AddUniqueDynamic(this, &ThisClass::OnSettingButtonClicked);
	}

	if (Btn_Close)
	{
		Btn_Close->OnClicked.AddUniqueDynamic(this, &ThisClass::OnCloseButtonClicked);
	}

	if (Btn_PandoraUpgrade)
	{
		Btn_PandoraUpgrade->OnClicked.AddUniqueDynamic(
			this,
			&ThisClass::OnPandoraUpgradeButtonClicked);
	}

	if (Btn_CanvasExport)
	{
		Btn_CanvasExport->OnClicked.AddUniqueDynamic(this, &ThisClass::OnCanvasExportButtonClicked);
	}

	if (Btn_FaceDecal)
	{
		Btn_FaceDecal->OnClicked.AddUniqueDynamic(this, &ThisClass::OnFaceDecalButtonClicked);
	}

	if (Btn_Debug)
	{
		Btn_Debug->OnClicked.AddUniqueDynamic(
			this,
			&ThisClass::OnDebugButtonClicked);
	}

	BindLeftSkinPaintCanvasEvents();
	if (Btn_ClosePaint) Btn_ClosePaint->OnClicked.AddUniqueDynamic(this, &ThisClass::OnClosePaintClicked);
	SetCanvasExportButtonVisible(false);
	SetPandoraUpgradeButtonVisible(
		FocusedSection == EInfoUiSection::Pandora);
	MapPanel->RefreshButtonEnabledState();
	MapPanel->HideImmediately();
}

void UInfoWidget::NativeDestruct()
{
	ClosePandoraDrawerForNavigation();
	if (MapPanel)
	{
		MapPanel->Shutdown();
	}
	if (CharacterPreview)
	{
		CharacterPreview->Shutdown(bReturnCameraOnHide);
	}
	if (DetailPopup)
	{
		DetailPopup->Shutdown();
	}
	if (PaintCanvas)
	{
		PaintCanvas->Shutdown();
	}

	if (ProfileTabButton)
	{
		ProfileTabButton->OnClicked.RemoveDynamic(this, &ThisClass::OnProfileTabButtonClicked);
	}

	if (ItemTabButton)
	{
		ItemTabButton->OnClicked.RemoveDynamic(this, &ThisClass::OnItemTabButtonClicked);
	}

	if (SkinButton)
	{
		SkinButton->OnClicked.RemoveDynamic(this, &ThisClass::OnSkinButtonClicked);
	}

	if (PandoraButton)
	{
		PandoraButton->OnClicked.RemoveDynamic(this, &ThisClass::OnPandoraButtonClicked);
	}

	if (MapButton)
	{
		MapButton->OnClicked.RemoveDynamic(this, &ThisClass::OnMapButtonClicked);
	}

	if (Btn_Setting)
	{
		Btn_Setting->OnClicked.RemoveDynamic(this, &ThisClass::OnSettingButtonClicked);
	}

	if (Btn_Close)
	{
		Btn_Close->OnClicked.RemoveDynamic(this, &ThisClass::OnCloseButtonClicked);
	}

	if (Btn_PandoraUpgrade)
	{
		Btn_PandoraUpgrade->OnClicked.RemoveDynamic(
			this,
			&ThisClass::OnPandoraUpgradeButtonClicked);
	}

	if (Btn_CanvasExport)
	{
		Btn_CanvasExport->OnClicked.RemoveDynamic(this, &ThisClass::OnCanvasExportButtonClicked);
	}

	if (Btn_FaceDecal)
	{
		Btn_FaceDecal->OnClicked.RemoveDynamic(this, &ThisClass::OnFaceDecalButtonClicked);
	}

	if (Btn_Debug)
	{
		Btn_Debug->OnClicked.RemoveDynamic(
			this,
			&ThisClass::OnDebugButtonClicked);
	}

	UnbindLeftSkinPaintCanvasEvents();
	if (Btn_ClosePaint) Btn_ClosePaint->OnClicked.RemoveDynamic(this, &ThisClass::OnClosePaintClicked);
	SetCanvasExportButtonVisible(false);

	Super::NativeDestruct();
}

bool UInfoWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	if (!IsScreenPositionInsideCharacterDropPanel(InDragDropEvent.GetScreenSpacePosition()))
	{
		return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
	}

	if (UItemSlotDragDropOperation* ItemDragOperation = Cast<UItemSlotDragDropOperation>(InOperation))
	{
		UItemInstance* DroppedItem = ItemDragOperation->GetItemInstance();
		if (DroppedItem)
		{
			OnDroppedItemToCharacterPanel.Broadcast(DroppedItem);
			return true;
		}
	}

	if (USkinSlotDragDropOperation* SkinDragOperation = Cast<USkinSlotDragDropOperation>(InOperation))
	{
		const USkinDefinition* DroppedSkin = SkinDragOperation->GetSkinDefinition();
		if (DroppedSkin)
		{
			OnDroppedSkinToCharacterPanel.Broadcast(DroppedSkin);
			return true;
		}
	}

	return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
}

FReply UInfoWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton
		&& PaintCanvas
		&& PaintCanvas->BeginStroke(InMouseEvent.GetScreenSpacePosition()))
	{
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UInfoWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (PaintCanvas
		&& PaintCanvas->ContinueStroke(
			InMouseEvent.GetScreenSpacePosition(),
			InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton)))
	{
		return FReply::Handled();
	}

	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UInfoWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton
		&& PaintCanvas
		&& PaintCanvas->EndStroke(InMouseEvent.GetScreenSpacePosition()))
	{
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

void UInfoWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	if (PaintCanvas)
	{
		PaintCanvas->CancelStroke();
	}
	Super::NativeOnMouseLeave(InMouseEvent);
}

void UInfoWidget::ShowItemDetailAtWidget(UItemInstance* ItemInstance, UWidget* AnchorWidget, const bool bPlaceLeftOfWidget)
{
	EnsureInfoViews();
	ConfigureInfoViews();
	DetailPopup->ShowItem(ItemInstance, AnchorWidget, bPlaceLeftOfWidget);
}

void UInfoWidget::ShowSkinDefinitionDetailAtWidget(const USkinDefinition* SkinDefinition, UWidget* AnchorWidget, const bool bPlaceLeftOfWidget)
{
	EnsureInfoViews();
	ConfigureInfoViews();
	DetailPopup->ShowSkinDefinition(SkinDefinition, AnchorWidget, bPlaceLeftOfWidget);
}

void UInfoWidget::ShowPandoraDescriptionDetailAtWidget(const UPandoraDefinition* PandoraDefinition, UWidget* AnchorWidget, const bool bPlaceLeftOfWidget)
{
	EnsureInfoViews();
	ConfigureInfoViews();
	DetailPopup->ShowPandora(PandoraDefinition, AnchorWidget, bPlaceLeftOfWidget, true);
}

void UInfoWidget::ShowPandoraDescriptionDetailImmediatelyAtWidget(
	const UPandoraDefinition* PandoraDefinition,
	UWidget* AnchorWidget,
	const bool bPlaceLeftOfWidget)
{
	EnsureInfoViews();
	ConfigureInfoViews();
	DetailPopup->ShowPandora(PandoraDefinition, AnchorWidget, bPlaceLeftOfWidget, false);
}

void UInfoWidget::HideDetailWidgets()
{
	if (DetailPopup)
	{
		DetailPopup->HideAll();
	}
}

void UInfoWidget::HidePandoraDescriptionDetailAtWidget(const UWidget* AnchorWidget)
{
	if (DetailPopup)
	{
		DetailPopup->HidePandoraForAnchor(AnchorWidget);
	}
}

void UInfoWidget::SelectProfileTab()
{
	FocusedSection = EInfoUiSection::Profile;
	SetPandoraUpgradeButtonVisible(false);
	if (MapPanel)
	{
		MapPanel->PlaySlideOut();
	}
	if (WB_LeftProfile)
	{
		WB_LeftProfile->RefreshTierImage();
	}

	SelectInfoCenterPage(
		WB_LeftProfile,
		WB_RightStatus,
		GetProfileLeftUiTag(),
		GetProfileRightUiTag());
}

void UInfoWidget::SelectItemTab()
{
	FocusedSection = EInfoUiSection::Item;
	SetPandoraUpgradeButtonVisible(false);
	if (MapPanel)
	{
		MapPanel->PlaySlideOut();
	}
	if (WB_RightInventory)
	{
		WB_RightInventory->ResetFilterHighlightToAll();
	}
	SelectInfoCenterPage(
		WB_LeftEquipment,
		WB_RightInventory,
		GetItemLeftUiTag(),
		GetItemRightUiTag());
}

void UInfoWidget::SelectSkinTab()
{
	FocusedSection = EInfoUiSection::Skin;
	SetPandoraUpgradeButtonVisible(false);
	if (MapPanel)
	{
		MapPanel->PlaySlideOut();
	}
	if (WB_RightSkin)
	{
		WB_RightSkin->ResetFilterHighlightToAll();
	}
	SelectInfoCenterPage(
		WB_LeftSkin,
		WB_RightSkin,
		GetSkinLeftUiTag(),
		GetSkinRightUiTag());
}

void UInfoWidget::SelectPandoraTab()
{
	FocusedSection = EInfoUiSection::Pandora;
	SetPandoraUpgradeButtonVisible(true);
	if (MapPanel)
	{
		MapPanel->PlaySlideOut();
	}
	if (WB_RightPandora)
	{
		WB_RightPandora->ResetFilterHighlightToAll();
	}
	SelectInfoCenterPage(
		WB_LeftPandora,
		WB_RightPandora,
		GetPandoraLeftUiTag(),
		GetPandoraRightUiTag());
}

void UInfoWidget::SelectMapTab()
{
	EnsureInfoViews();
	ConfigureInfoViews();
	if (MapPanel->IsDisabledForCurrentMap())
	{
		MapPanel->HideImmediately();
		MapPanel->RefreshButtonEnabledState();
		return;
	}

	FocusedSection = EInfoUiSection::Map;
	SetPandoraUpgradeButtonVisible(false);
	HideDetailWidgets();
	HideSkinPaintCanvasGroup();
	PlaySidePanelsSlideOutAnimation();
	MapPanel->PlaySlideIn();
	OnClickedMapButton.Broadcast();
}

void UInfoWidget::FocusSection(
	const EInfoUiSection Section,
	const bool bAnimateTransition)
{
	switch (Section)
	{
	case EInfoUiSection::Profile:
		SelectProfileTab();
		break;
	case EInfoUiSection::Item:
		SelectItemTab();
		break;
	case EInfoUiSection::Skin:
		SelectSkinTab();
		break;
	case EInfoUiSection::Pandora:
		SelectPandoraTab();
		break;
	case EInfoUiSection::Map:
		SelectMapTab();
		return;
	}

	if (bAnimateTransition)
	{
		PlaySidePanelsSlideInAnimation();
	}
}

void UInfoWidget::ShowInfoUi()
{
	StopAllAnimations();
	EnsureInfoViews();
	ConfigureInfoViews();

	SetIsFocusable(true);
	SetVisibility(ESlateVisibility::Visible);
	MapPanel->RefreshButtonEnabledState();
	MapPanel->EnsureTotalMapWidget();
	MapPanel->HideImmediately();

	int32 PlayedAnimationCount = 0;
	if (SlideInLeft)
	{
		PlayAnimation(SlideInLeft, 0.0f, 1, EUMGSequencePlayMode::Forward, 1.0f, false);
		++PlayedAnimationCount;
	}

	if (SlideInRight)
	{
		PlayAnimation(SlideInRight, 0.0f, 1, EUMGSequencePlayMode::Forward, 1.0f, false);
		++PlayedAnimationCount;
	}

	if (SlideInBottom)
	{
		PlayAnimation(SlideInBottom, 0.0f, 1, EUMGSequencePlayMode::Forward, 1.0f, false);
		++PlayedAnimationCount;
	}

CharacterPreview->ShowPreview();
}

void UInfoWidget::HideInfoUi()
{
	ClosePandoraDrawerForNavigation();
	StopAllAnimations();
	HideSkinPaintCanvasGroup();
	if (bReturnCameraOnHide && CharacterPreview)
	{
		CharacterPreview->ReturnCameraToPawn();
	}

	int32 PlayedAnimationCount = 0;
	if (SlideInLeft)
	{
		PlayAnimationReverse(SlideInLeft, 1.0f, false);
		++PlayedAnimationCount;
	}

	if (SlideInRight)
	{
		PlayAnimationReverse(SlideInRight, 1.0f, false);
		++PlayedAnimationCount;
	}

	if (SlideInBottom)
	{
		PlayAnimationReverse(SlideInBottom, 1.0f, false);
		++PlayedAnimationCount;
	}

	const bool bShouldCloseMap = MapPanel && MapPanel->IsOpenOrVisible();
	if (bShouldCloseMap)
	{
		MapPanel->PlaySlideOut();
		++PlayedAnimationCount;
	}

}

float UInfoWidget::GetHideAnimationDelay() const
{
	const bool bShouldWaitMap = MapPanel && MapPanel->IsOpenOrVisible();
	const bool bHasAnyHideAnimation = SlideInLeft || SlideInRight || SlideInBottom || bShouldWaitMap;
	if (!bHasAnyHideAnimation)
	{
		return 0.0f;
	}

	const float MapDelay = bShouldWaitMap ? MapPanel->GetHideAnimationDelay() : 0.0f;
	return FMath::Max(HideAnimationDelay, MapDelay);
}

URightInventoryWidget* UInfoWidget::GetRightInventoryWidget() const
{
	return WB_RightInventory;
}

URightStatusWidget* UInfoWidget::GetRightStatusWidget() const
{
	return WB_RightStatus;
}

ULeftEquipmentWidget* UInfoWidget::GetLeftEquipmentWidget() const
{
	return WB_LeftEquipment;
}

ULeftSkinWidget* UInfoWidget::GetLeftSkinWidget() const
{
	return WB_LeftSkin;
}

ULeftPandoraWidget* UInfoWidget::GetLeftPandoraWidget() const
{
	return WB_LeftPandora;
}

URightSkinWidget* UInfoWidget::GetRightSkinWidget() const
{
	return WB_RightSkin;
}

URightPandoraWidget* UInfoWidget::GetRightPandoraWidget() const
{
	return WB_RightPandora;
}

void UInfoWidget::OnProfileTabButtonClicked()
{
	SelectProfileTab();
	PlaySidePanelsSlideInAnimation();
}

void UInfoWidget::OnItemTabButtonClicked()
{
	SelectItemTab();
	PlaySidePanelsSlideInAnimation();
}

void UInfoWidget::OnSkinButtonClicked()
{
	SelectSkinTab();
	PlaySidePanelsSlideInAnimation();
}

void UInfoWidget::OnPandoraButtonClicked()
{
	SelectPandoraTab();
	PlaySidePanelsSlideInAnimation();
}

void UInfoWidget::OnMapButtonClicked()
{
	SelectMapTab();
}

void UInfoWidget::OnSettingButtonClicked()
{
	if (MapPanel)
	{
		MapPanel->PlaySlideOut();
	}
	HideSkinPaintCanvasGroup();
	OnClickedSettingButton.Broadcast();
}

void UInfoWidget::OnCloseButtonClicked()
{
	CloseGameSettings();
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		if (APdHUD* Hud = PlayerController->GetHUD<APdHUD>())
		{
			Hud->CloseInfoUi();
		}
	}
}

void UInfoWidget::OnPandoraUpgradeButtonClicked()
{
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		if (APdHUD* Hud = PlayerController->GetHUD<APdHUD>())
		{
			const TWeakObjectPtr<APdHUD> WeakHud = Hud;
			FTSTicker::GetCoreTicker().AddTicker(
				FTickerDelegate::CreateLambda(
					[WeakHud](float)
					{
						if (APdHUD* ValidHud = WeakHud.Get())
						{
							ValidHud->TogglePandoraTreeUi();
						}

						return false;
					}));
		}
	}
}

void UInfoWidget::OnCanvasExportButtonClicked()
{
	if (!PaintCanvas)
	{
		SetPaintCanvasWidgetVisible(false);
		SetCanvasExportButtonVisible(false);
		return;
	}

	PaintCanvas->ExportActiveCanvas();
	SetCanvasExportButtonVisible(PaintCanvas->HasActiveCanvas());
}

void UInfoWidget::OnFaceDecalButtonClicked()
{
	if (!PaintCanvas)
	{
		SetCanvasExportButtonVisible(false);
		return;
	}

	PaintCanvas->ApplyActiveCanvasToFaceDecal();
	SetCanvasExportButtonVisible(PaintCanvas->HasActiveCanvas());
}

void UInfoWidget::OnDebugButtonClicked()
{
	UPlayerProfileSubsystem* ProfileSubsystem = UGameInstance::GetSubsystem<UPlayerProfileSubsystem>(GetGameInstance());
	if (!ProfileSubsystem)
	{
		return;
	}

	constexpr int32 DebugVictoryGold = 123;
	FMatchRecord VictoryRecord;
	VictoryRecord.bWin = true;
	VictoryRecord.Reward = DebugVictoryGold;
	ProfileSubsystem->AddMatchRecord(VictoryRecord, false);
	ProfileSubsystem->AddGold(DebugVictoryGold, true);

	if (WB_LeftProfile)
	{
		WB_LeftProfile->RefreshTierImage();
	}
}

void UInfoWidget::ApplyWidgetDefinitionSettings()
{
	if (const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this))
	{
		const FInfoWidgetSettings& Settings = WidgetDefinition->GetInfoWidgetSettings();
		HideAnimationDelay = Settings.HideAnimationDelay;
		MapSlideStartOffset = Settings.MapSlideStartOffset;
		MapSlideDuration = Settings.MapSlideDuration;
		bUseCharacterPreviewCamera = Settings.bUseCharacterPreviewCamera;
		bReturnCameraOnHide = Settings.bReturnCameraOnHide;
		DetailPopupOffset = Settings.DetailPopupOffset;
		if (const TSubclassOf<UMapWidget> ResolvedTotalMapWidgetClass =
			WidgetDefinition->GetTotalMapWidgetClass())
		{
			TotalMapWidgetClass = ResolvedTotalMapWidgetClass;
		}
		if (const TSubclassOf<AActor> ResolvedCharacterPreviewClass =
			WidgetDefinition->GetCharacterPreviewClass())
		{
			CharacterPreviewClass = ResolvedCharacterPreviewClass;
		}
		if (const TSubclassOf<UPandoraDescriptionWidget> ResolvedPandoraDescriptionWidgetClass =
			WidgetDefinition->GetPandoraDescriptionWidgetClass())
		{
			PandoraDescriptionWidgetClass = ResolvedPandoraDescriptionWidgetClass;
		}
	}
}

void UInfoWidget::HandlePaintCanvasGroupVisibilityChanged(const bool bVisible)
{
	const bool bPaintCanvasVisible = SetPaintCanvasWidgetVisible(bVisible);
	SetCanvasExportButtonVisible(bPaintCanvasVisible);
}

void UInfoWidget::PlaySidePanelsSlideInAnimation()
{
	int32 PlayedAnimationCount = 0;
	if (SlideInLeft)
	{
		StopAnimation(SlideInLeft);
		PlayAnimation(SlideInLeft, 0.0f, 1, EUMGSequencePlayMode::Forward, 1.0f, false);
		++PlayedAnimationCount;
	}

	if (SlideInRight)
	{
		StopAnimation(SlideInRight);
		PlayAnimation(SlideInRight, 0.0f, 1, EUMGSequencePlayMode::Forward, 1.0f, false);
		++PlayedAnimationCount;
	}

}

void UInfoWidget::PlaySidePanelsSlideOutAnimation()
{
	int32 PlayedAnimationCount = 0;
	if (SlideInLeft)
	{
		StopAnimation(SlideInLeft);
		PlayAnimationReverse(SlideInLeft, 1.0f, false);
		++PlayedAnimationCount;
	}

	if (SlideInRight)
	{
		StopAnimation(SlideInRight);
		PlayAnimationReverse(SlideInRight, 1.0f, false);
		++PlayedAnimationCount;
	}

}

void UInfoWidget::SelectInfoCenterPage(UWidget* LeftWidget, UWidget* RightWidget, const FGameplayTag& LeftUiTag, const FGameplayTag& RightUiTag)
{
	HideDetailWidgets();
	if (LeftUiTag != GetSkinLeftUiTag() && RightUiTag != GetSkinRightUiTag())
	{
		HideSkinPaintCanvasGroup();
	}

	if (LeftWidgetSwitcher && LeftWidget)
	{
		LeftWidgetSwitcher->SetActiveWidget(LeftWidget);
	}

	if (RightWidgetSwitcher && RightWidget)
	{
		RightWidgetSwitcher->SetActiveWidget(RightWidget);
	}

	OnClickedInfoCenterButton.Broadcast(LeftUiTag, RightUiTag);
}

void UInfoWidget::HideSkinPaintCanvasGroup()
{
	if (WB_LeftSkin)
	{
		WB_LeftSkin->HidePaintCanvasGroup();
	}

	SetPaintCanvasWidgetVisible(false);
	SetCanvasExportButtonVisible(false);
}

void UInfoWidget::OnClosePaintClicked()
{
	HideSkinPaintCanvasGroup();
}

bool UInfoWidget::SetPaintCanvasWidgetVisible(const bool bVisible)
{
	const bool bPaintCanvasVisible = PaintCanvas
		&& PaintCanvas->SetVisible(bVisible);

	if (Txt_Canvas)
	{
		Txt_Canvas->SetVisibility(bPaintCanvasVisible
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	}

	return bPaintCanvasVisible;
}

void UInfoWidget::SetCanvasExportButtonVisible(const bool bVisible) const
{
	if (Btn_ClosePaint) Btn_ClosePaint->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (Btn_CanvasExport)
	{
		Btn_CanvasExport->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (Btn_FaceDecal)
	{
		Btn_FaceDecal->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UInfoWidget::SetPandoraUpgradeButtonVisible(const bool bVisible)
{
	if (!bVisible) ClosePandoraDrawerForNavigation();
	RefreshPandoraDrawerLabel();
	if (Btn_PandoraUpgrade)
	{
		Btn_PandoraUpgrade->SetVisibility(
			bVisible
				? ESlateVisibility::Visible
				: ESlateVisibility::Collapsed);
	}
}

bool UInfoWidget::AttachPandoraTree(UPandoraTreeWidget* Tree)
{
	if (!PandoraTreeHost || !Tree) return false;
	HideDetailWidgets();
	PandoraTreeHost->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (Tree->GetParent() != PandoraTreeHost)
	{
		UCanvasPanelSlot* TreeSlot = PandoraTreeHost->AddChildToCanvas(Tree);
		TreeSlot->SetAnchors(FAnchors(0, 0, 1, 1));
		TreeSlot->SetOffsets(FMargin(0));
	}
	AttachedPandoraTree = Tree;
	bPandoraDrawerExpanded = true;
	if (RightWidgetSwitcher) RightWidgetSwitcher->SetVisibility(ESlateVisibility::Hidden);
	if (CharacterPanel) CharacterPanel->SetVisibility(ESlateVisibility::Hidden);
	if (UWidget* Memo = GetWidgetFromName(TEXT("Txt_Memo_LocaleFit"))) Memo->SetVisibility(ESlateVisibility::Hidden);
	RefreshPandoraDrawerLabel();
	return true;
}

void UInfoWidget::OnPandoraDrawerClosed()
{
	AttachedPandoraTree.Reset();
	bPandoraDrawerExpanded = false;
	if (PandoraTreeHost) PandoraTreeHost->SetVisibility(ESlateVisibility::Collapsed);
	if (RightWidgetSwitcher) RightWidgetSwitcher->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (CharacterPanel) CharacterPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (UWidget* Memo = GetWidgetFromName(TEXT("Txt_Memo_LocaleFit"))) Memo->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	RefreshPandoraDrawerLabel();
}

void UInfoWidget::ClosePandoraDrawerForNavigation()
{
	if (!bPandoraDrawerExpanded) return;
	if (UPandoraTreeWidget* Tree = AttachedPandoraTree.Get()) Tree->HidePandoraTreeImmediately();
}

void UInfoWidget::RefreshPandoraDrawerLabel()
{
	const FText Label = MenuTextOrFallback(bPandoraDrawerExpanded ? TEXT("Pandora.Collapse") : TEXT("Pandora.Expand"),
		FText::FromString(bPandoraDrawerExpanded ? TEXT("Collapse") : TEXT("Expand")));
	if (PandoraDrawerAction) PandoraDrawerAction->SetText(Label);
	if (PandoraDrawerArrow) PandoraDrawerArrow->SetText(FText::FromString(bPandoraDrawerExpanded ? TEXT("‹") : TEXT("›")));
	if (Btn_PandoraUpgrade) Btn_PandoraUpgrade->SetToolTipText(MenuTextOrFallback(
		bPandoraDrawerExpanded ? TEXT("Pandora.CollapseHint") : TEXT("Pandora.ExpandHint"), Label));
}

void UInfoWidget::OnMenuLanguageChanged()
{
	Super::OnMenuLanguageChanged();
	RefreshPandoraDrawerLabel();
}

void UInfoWidget::BindLeftSkinPaintCanvasEvents()
{
	if (WB_LeftSkin)
	{
		WB_LeftSkin->OnPaintCanvasGroupVisibilityChanged.AddUniqueDynamic(
			this,
			&ThisClass::HandlePaintCanvasGroupVisibilityChanged);
	}
}

void UInfoWidget::UnbindLeftSkinPaintCanvasEvents()
{
	if (WB_LeftSkin)
	{
		WB_LeftSkin->OnPaintCanvasGroupVisibilityChanged.RemoveDynamic(
			this,
			&ThisClass::HandlePaintCanvasGroupVisibilityChanged);
	}
}

bool UInfoWidget::IsScreenPositionInsideCharacterDropPanel(const FVector2D& ScreenSpacePosition) const
{
	const UWidget* DropPanel = CharacterPanel ? CharacterPanel.Get() : CenterPreviewPanel.Get();
	if (!DropPanel || bPandoraDrawerExpanded)
	{
		return false;
	}

	const FGeometry& PanelGeometry = DropPanel->GetCachedGeometry();
	const FVector2D LocalPosition = PanelGeometry.AbsoluteToLocal(ScreenSpacePosition);
	const FVector2D LocalSize = PanelGeometry.GetLocalSize();
	return LocalPosition.X >= 0.0f &&
		LocalPosition.Y >= 0.0f &&
		LocalPosition.X <= LocalSize.X &&
		LocalPosition.Y <= LocalSize.Y;
}

FGameplayTag UInfoWidget::GetProfileLeftUiTag() const
{
	return UProjectTagDefinition::Get(this)->GetUiProfileLeftTag();
}

FGameplayTag UInfoWidget::GetProfileRightUiTag() const
{
	return UProjectTagDefinition::Get(this)->GetUiStatusRightTag();
}

FGameplayTag UInfoWidget::GetItemLeftUiTag() const
{
	return UProjectTagDefinition::Get(this)->GetUiEquipmentLeftTag();
}

FGameplayTag UInfoWidget::GetItemRightUiTag() const
{
	return UProjectTagDefinition::Get(this)->GetUiInventoryRightTag();
}

FGameplayTag UInfoWidget::GetSkinLeftUiTag() const
{
	return UProjectTagDefinition::Get(this)->GetUiSkinEquipmentLeftTag();
}

FGameplayTag UInfoWidget::GetSkinRightUiTag() const
{
	return UProjectTagDefinition::Get(this)->GetUiSkinInventoryRightTag();
}

FGameplayTag UInfoWidget::GetPandoraLeftUiTag() const
{
	return UProjectTagDefinition::Get(this)->GetUiPandoraEquipmentLeftTag();
}

FGameplayTag UInfoWidget::GetPandoraRightUiTag() const
{
	return UProjectTagDefinition::Get(this)->GetUiPandoraInventoryRightTag();
}
