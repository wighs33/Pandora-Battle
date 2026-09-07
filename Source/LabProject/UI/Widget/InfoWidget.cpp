#include "UI/Widget/InfoWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetTree.h"
#include "Containers/Ticker.h"
#include "Definition/Common/ProjectTagConfig.h"
#include "Components/Button.h"
#include "Components/Overlay.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Item/ItemInstance.h"
#include "Engine/GameInstance.h"
#include "SavedGameData/PlayerProfileSubsystem.h"
#include "Mode/PdHUD.h"
#include "Pandora/PandoraInstance.h"
#include "Skin/SkinInstance.h"
#include "Definition/UI/WidgetClassDefinition.h"
#include "UI/Controller/InfoCharacterPreviewController.h"
#include "UI/Controller/InfoDetailController.h"
#include "UI/Controller/InfoMapController.h"
#include "UI/Controller/InfoPaintCanvasController.h"
#include "UI/Widget/ItemSlotDragDropOperation.h"
#include "UI/Widget/LeftEquipmentWidget.h"
#include "UI/Widget/LeftPandoraWidget.h"
#include "UI/Widget/LeftProfileWidget.h"
#include "UI/Widget/LeftSkinWidget.h"
#include "UI/Widget/MapWidget.h"
#include "UI/Widget/PandoraDescriptionWidget.h"
#include "UI/Widget/PaintCanvasWidget.h"
#include "UI/Widget/RightInventoryWidget.h"
#include "UI/Widget/RightPandoraWidget.h"
#include "UI/Widget/RightSkinWidget.h"
#include "UI/Widget/RightStatusWidget.h"
#include "UI/Widget/SkinSlotDragDropOperation.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InfoWidget)

UInfoWidget::UInfoWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UInfoWidget::EnsureControllers()
{
	if (!DetailController)
	{
		DetailController = NewObject<UInfoDetailController>(this);
	}
	if (!CharacterPreviewController)
	{
		CharacterPreviewController = NewObject<UInfoCharacterPreviewController>(this);
	}
	if (!MapController)
	{
		MapController = NewObject<UInfoMapController>(this);
	}
	if (!PaintCanvasController)
	{
		PaintCanvasController = NewObject<UInfoPaintCanvasController>(this);
	}
}

void UInfoWidget::ConfigureControllers()
{
	if (DetailController)
	{
		DetailController->Initialize(
			this,
			WB_LeftEquipment,
			ItemDetailWidgetClass,
			PandoraDescriptionWidgetClass,
			DetailPopupOffset);
	}
	if (CharacterPreviewController)
	{
		CharacterPreviewController->Initialize(
			this,
			bUseCharacterPreviewCamera,
			CharacterPreviewClass);
	}
	if (MapController)
	{
		MapController->Initialize(
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
	if (PaintCanvasController)
	{
		PaintCanvasController->Initialize(this, Canvas);
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
	EnsureControllers();
	ConfigureControllers();
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
	SetCanvasExportButtonVisible(false);
	SetPandoraUpgradeButtonVisible(
		FocusedSection == EInfoUiSection::Pandora);
	MapController->RefreshButtonEnabledState();
	MapController->HideImmediately();
}

void UInfoWidget::NativeDestruct()
{
	if (MapController)
	{
		MapController->Shutdown();
	}
	if (CharacterPreviewController)
	{
		CharacterPreviewController->Shutdown(bReturnCameraOnHide);
	}
	if (DetailController)
	{
		DetailController->Shutdown();
	}
	if (PaintCanvasController)
	{
		PaintCanvasController->Shutdown();
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
		USkinInstance* DroppedSkin = SkinDragOperation->GetSkinInstance();
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
		&& PaintCanvasController
		&& PaintCanvasController->BeginStroke(InMouseEvent.GetScreenSpacePosition()))
	{
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UInfoWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (PaintCanvasController
		&& PaintCanvasController->ContinueStroke(
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
		&& PaintCanvasController
		&& PaintCanvasController->EndStroke(InMouseEvent.GetScreenSpacePosition()))
	{
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

void UInfoWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	if (PaintCanvasController)
	{
		PaintCanvasController->CancelStroke();
	}
	Super::NativeOnMouseLeave(InMouseEvent);
}

void UInfoWidget::ShowItemDetailAtWidget(UItemInstance* ItemInstance, UWidget* AnchorWidget, const bool bPlaceLeftOfWidget)
{
	EnsureControllers();
	ConfigureControllers();
	DetailController->ShowItem(ItemInstance, AnchorWidget, bPlaceLeftOfWidget);
}

void UInfoWidget::ShowSkinDetailAtWidget(USkinInstance* SkinInstance, UWidget* AnchorWidget, const bool bPlaceLeftOfWidget)
{
	EnsureControllers();
	ConfigureControllers();
	DetailController->ShowSkin(SkinInstance, AnchorWidget, bPlaceLeftOfWidget);
}

void UInfoWidget::ShowSkinDefinitionDetailAtWidget(const USkinDefinition* SkinDefinition, UWidget* AnchorWidget, const bool bPlaceLeftOfWidget)
{
	EnsureControllers();
	ConfigureControllers();
	DetailController->ShowSkinDefinition(SkinDefinition, AnchorWidget, bPlaceLeftOfWidget);
}

void UInfoWidget::ShowPandoraDescriptionDetailAtWidget(UPandoraInstance* PandoraInstance, UWidget* AnchorWidget, const bool bPlaceLeftOfWidget)
{
	EnsureControllers();
	ConfigureControllers();
	DetailController->ShowPandora(PandoraInstance, AnchorWidget, bPlaceLeftOfWidget, true);
}

void UInfoWidget::ShowPandoraDescriptionDetailImmediatelyAtWidget(
	UPandoraInstance* PandoraInstance,
	UWidget* AnchorWidget,
	const bool bPlaceLeftOfWidget)
{
	EnsureControllers();
	ConfigureControllers();
	DetailController->ShowPandora(PandoraInstance, AnchorWidget, bPlaceLeftOfWidget, false);
}

void UInfoWidget::HideDetailWidgets()
{
	if (DetailController)
	{
		DetailController->HideAll();
	}
}

void UInfoWidget::HidePandoraDescriptionDetailAtWidget(const UWidget* AnchorWidget)
{
	if (DetailController)
	{
		DetailController->HidePandoraForAnchor(AnchorWidget);
	}
}

void UInfoWidget::SelectProfileTab()
{
	FocusedSection = EInfoUiSection::Profile;
	SetPandoraUpgradeButtonVisible(false);
	if (MapController)
	{
		MapController->PlaySlideOut();
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
	if (MapController)
	{
		MapController->PlaySlideOut();
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
	if (MapController)
	{
		MapController->PlaySlideOut();
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
	if (MapController)
	{
		MapController->PlaySlideOut();
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
	EnsureControllers();
	ConfigureControllers();
	if (MapController->IsDisabledForCurrentMap())
	{
		MapController->HideImmediately();
		MapController->RefreshButtonEnabledState();
		return;
	}

	FocusedSection = EInfoUiSection::Map;
	SetPandoraUpgradeButtonVisible(false);
	HideDetailWidgets();
	HideSkinPaintCanvasGroup();
	PlaySidePanelsSlideOutAnimation();
	MapController->PlaySlideIn();
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
	EnsureControllers();
	ConfigureControllers();

	SetIsFocusable(true);
	SetVisibility(ESlateVisibility::Visible);
	SetFocus();
	MapController->RefreshButtonEnabledState();
	MapController->EnsureTotalMapWidget();
	MapController->HideImmediately();

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

CharacterPreviewController->ShowPreview();
}

void UInfoWidget::HideInfoUi()
{
	StopAllAnimations();
	HideSkinPaintCanvasGroup();
	if (bReturnCameraOnHide && CharacterPreviewController)
	{
		CharacterPreviewController->ReturnCameraToPawn();
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

	const bool bShouldCloseMap = MapController && MapController->IsOpenOrVisible();
	if (bShouldCloseMap)
	{
		MapController->PlaySlideOut();
		++PlayedAnimationCount;
	}

}

float UInfoWidget::GetHideAnimationDelay() const
{
	const bool bShouldWaitMap = MapController && MapController->IsOpenOrVisible();
	const bool bHasAnyHideAnimation = SlideInLeft || SlideInRight || SlideInBottom || bShouldWaitMap;
	if (!bHasAnyHideAnimation)
	{
		return 0.0f;
	}

	const float MapDelay = bShouldWaitMap ? MapController->GetHideAnimationDelay() : 0.0f;
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
	if (MapController)
	{
		MapController->PlaySlideOut();
	}
	HideSkinPaintCanvasGroup();
	OnClickedSettingButton.Broadcast();
}

void UInfoWidget::OnCloseButtonClicked()
{
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
	if (!PaintCanvasController)
	{
		SetPaintCanvasWidgetVisible(false);
		SetCanvasExportButtonVisible(false);
		return;
	}

	const bool bExported = PaintCanvasController->ExportActiveCanvas();
	if (bExported)
	{
		SetPaintCanvasWidgetVisible(false);
		OnCloseButtonClicked();
		return;
	}
	SetCanvasExportButtonVisible(
		PaintCanvasController->HasActiveCanvas());
}

void UInfoWidget::OnFaceDecalButtonClicked()
{
	if (!PaintCanvasController)
	{
		SetCanvasExportButtonVisible(false);
		return;
	}

	PaintCanvasController->ApplyActiveCanvasToFaceDecal();
	SetCanvasExportButtonVisible(PaintCanvasController->HasActiveCanvas());
}

void UInfoWidget::OnDebugButtonClicked()
{
	UPlayerProfileSubsystem* ProfileSubsystem = UGameInstance::GetSubsystem<UPlayerProfileSubsystem>(GetGameInstance());
	if (!ProfileSubsystem)
	{
		return;
	}

	const FString PlayerId = ProfileSubsystem->GetLocalClientSavePlayerId();
	if (PlayerId.IsEmpty())
	{
		return;
	}

	constexpr int32 DebugVictoryGold = 123;
	FMatchRecord VictoryRecord;
	VictoryRecord.bWin = true;
	VictoryRecord.Reward = DebugVictoryGold;
	ProfileSubsystem->AddMatchRecord(PlayerId, VictoryRecord, false);
	ProfileSubsystem->AddGold(PlayerId, DebugVictoryGold, true);

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

bool UInfoWidget::SetPaintCanvasWidgetVisible(const bool bVisible)
{
	const bool bPaintCanvasVisible = PaintCanvasController
		&& PaintCanvasController->SetVisible(bVisible);

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
	if (Btn_CanvasExport)
	{
		Btn_CanvasExport->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (Btn_FaceDecal)
	{
		Btn_FaceDecal->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UInfoWidget::SetPandoraUpgradeButtonVisible(const bool bVisible) const
{
	if (Btn_PandoraUpgrade)
	{
		Btn_PandoraUpgrade->SetVisibility(
			bVisible
				? ESlateVisibility::Visible
				: ESlateVisibility::Collapsed);
	}
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
	if (!DropPanel)
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
	return UProjectTagConfig::Get(this)->GetUiProfileLeftTag();
}

FGameplayTag UInfoWidget::GetProfileRightUiTag() const
{
	return UProjectTagConfig::Get(this)->GetUiStatusRightTag();
}

FGameplayTag UInfoWidget::GetItemLeftUiTag() const
{
	return UProjectTagConfig::Get(this)->GetUiEquipmentLeftTag();
}

FGameplayTag UInfoWidget::GetItemRightUiTag() const
{
	return UProjectTagConfig::Get(this)->GetUiInventoryRightTag();
}

FGameplayTag UInfoWidget::GetSkinLeftUiTag() const
{
	return UProjectTagConfig::Get(this)->GetUiSkinEquipmentLeftTag();
}

FGameplayTag UInfoWidget::GetSkinRightUiTag() const
{
	return UProjectTagConfig::Get(this)->GetUiSkinInventoryRightTag();
}

FGameplayTag UInfoWidget::GetPandoraLeftUiTag() const
{
	return UProjectTagConfig::Get(this)->GetUiPandoraEquipmentLeftTag();
}

FGameplayTag UInfoWidget::GetPandoraRightUiTag() const
{
	return UProjectTagConfig::Get(this)->GetUiPandoraInventoryRightTag();
}
