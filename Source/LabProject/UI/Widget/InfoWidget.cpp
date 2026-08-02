#include "UI/Widget/InfoWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Character/PdPlayer.h"
#include "Definition/Common/ProjectTagConfig.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SizeBox.h"
#include "Components/WidgetSwitcher.h"
#include "Data/ContentDataSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Item/ItemInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Definition/Match/MatchRuleDefinition.h"
#include "Mode/PdHUD.h"
#include "Mode/PdGameInstance.h"
#include "Mode/PdPlayerController.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "Pandora/PandoraInstance.h"
#include "Skin/SkinInstance.h"
#include "Definition/UI/WidgetClassDefinition.h"
#include "UI/Widget/EquipSlotWidget.h"
#include "UI/Widget/ItemDetailWidget.h"
#include "UI/Widget/ItemSlotDragDropOperation.h"
#include "UI/Widget/MapWidget.h"
#include "UI/Widget/PandoraDescriptionWidget.h"
#include "UI/Widget/SkinSlotDragDropOperation.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InfoWidget)

namespace
{
	void CutToViewTarget(APlayerController* PlayerController, AActor* ViewTarget)
	{
		if (!PlayerController || !IsValid(ViewTarget))
		{
			return;
		}

		PlayerController->SetViewTarget(ViewTarget);
		if (PlayerController->PlayerCameraManager)
		{
			PlayerController->PlayerCameraManager->UpdateCamera(0.0f);
			PlayerController->PlayerCameraManager->SetGameCameraCutThisFrame();
		}
	}

	void DisablePreviewCameraLetterboxing(AActor* ViewTarget)
	{
		if (!IsValid(ViewTarget))
		{
			return;
		}

		TArray<UCameraComponent*> CameraComponents;
		ViewTarget->GetComponents<UCameraComponent>(CameraComponents);
		for (UCameraComponent* CameraComponent : CameraComponents)
		{
			if (!IsValid(CameraComponent))
			{
				continue;
			}

			CameraComponent->SetConstraintAspectRatio(false);
			CameraComponent->bOverrideAspectRatioAxisConstraint = false;
		}
	}

	float GetAnimationDuration(const UWidgetAnimation* Animation, const float FallbackDuration)
	{
		if (!Animation)
		{
			return FallbackDuration;
		}

		return FMath::Max(Animation->GetEndTime() - Animation->GetStartTime(), FallbackDuration);
	}

	const TSoftObjectPtr<UMatchRuleDefinition>& GetDefaultMapUiMatchRuleDefinition()
	{
		static const TSoftObjectPtr<UMatchRuleDefinition> DefaultMatchRules(
			FSoftObjectPath(TEXT("/Game/Data/DA_MatchRule.DA_MatchRule")));
		return DefaultMatchRules;
	}

	const UMatchRuleDefinition* ResolveLoadedMapUiMatchRuleDefinition()
	{
		if (const UMatchRuleDefinition* LoadedMatchRules =
			GetDefaultMapUiMatchRuleDefinition().Get())
		{
			return LoadedMatchRules;
		}

		return GetDefault<UMatchRuleDefinition>();
	}

	FString StripTravelOptions(const FString& TravelMapName)
	{
		FString CleanMapName = TravelMapName;
		int32 OptionsIndex = INDEX_NONE;
		if (CleanMapName.FindChar(TEXT('?'), OptionsIndex))
		{
			CleanMapName.LeftInline(OptionsIndex, EAllowShrinking::No);
		}
		return CleanMapName;
	}

	bool DoesMapOptionMatchCurrentLevel(
		const FLobbyMatchMapOption& MapOption,
		const FString& CurrentPackageName,
		const FString& CurrentLevelName)
	{
		const FString MapPackageName = MapOption.Map.ToSoftObjectPath().GetLongPackageName();
		if (!MapPackageName.IsEmpty())
		{
			if (MapPackageName.Equals(CurrentPackageName, ESearchCase::IgnoreCase)
				|| FPackageName::GetShortName(MapPackageName).Equals(CurrentLevelName, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}

		const FString TravelMapName = StripTravelOptions(MapOption.TravelMapName);
		if (!TravelMapName.IsEmpty())
		{
			if (TravelMapName.Equals(CurrentPackageName, ESearchCase::IgnoreCase)
				|| FPackageName::GetShortName(TravelMapName).Equals(CurrentLevelName, ESearchCase::IgnoreCase)
				|| TravelMapName.Equals(CurrentLevelName, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}

		return false;
	}

	bool FindMapOptionForCurrentMapUi(const UInfoWidget* Widget, FLobbyMatchMapOption& OutMapOption)
	{
		if (!Widget)
		{
			return false;
		}

		const UMatchRuleDefinition* MatchRules = ResolveLoadedMapUiMatchRuleDefinition();
		if (!MatchRules || MatchRules->LobbyMapOptions.IsEmpty())
		{
			return false;
		}

		const UWorld* CurrentWorld = Widget->GetWorld();
		const FString CurrentPackageName = CurrentWorld && CurrentWorld->GetOutermost()
			? CurrentWorld->GetOutermost()->GetName()
			: FString();
		const FString CurrentLevelName = UGameplayStatics::GetCurrentLevelName(Widget, true);
		const bool bHasCurrentLevelContext = !CurrentPackageName.IsEmpty() || !CurrentLevelName.IsEmpty();

		if (const UWorld* World = Widget->GetWorld())
		{
			if (const UPdGameInstance* PdGameInstance = World->GetGameInstance<UPdGameInstance>())
			{
				const FName SelectedMapKey = PdGameInstance->GetLobbySelectedMapKey();
				if (!SelectedMapKey.IsNone()
					&& MatchRules->FindLobbyMapOption(SelectedMapKey, OutMapOption)
					&& !OutMapOption.GameplayMapWidgetClass.IsNull()
					&& (!bHasCurrentLevelContext
						|| DoesMapOptionMatchCurrentLevel(OutMapOption, CurrentPackageName, CurrentLevelName)))
				{
					return true;
				}
			}
		}

		for (const FLobbyMatchMapOption& MapOption : MatchRules->LobbyMapOptions)
		{
			if (MapOption.GameplayMapWidgetClass.IsNull())
			{
				continue;
			}

			if (DoesMapOptionMatchCurrentLevel(MapOption, CurrentPackageName, CurrentLevelName))
			{
				OutMapOption = MapOption;
				return true;
			}
		}

		return false;
	}
}

UInfoWidget::UInfoWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UInfoWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	ApplyWidgetDefinitionSettings();

	if (bAutoApplyInfoLayout)
	{
		ApplyInfoLayout();
	}
}

void UInfoWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ApplyWidgetDefinitionSettings();
	SetIsFocusable(true);

	if (bAutoApplyInfoLayout)
	{
		ApplyInfoLayout();
	}

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
#if UE_BUILD_SHIPPING
		Btn_Debug->SetVisibility(ESlateVisibility::Collapsed);
#else
		Btn_Debug->OnClicked.AddUniqueDynamic(this, &ThisClass::OnDebugButtonClicked);
#endif
	}

	BindLeftSkinPaintCanvasEvents();
	SetCanvasExportButtonVisible(false);
	SetPandoraUpgradeButtonVisible(
		FocusedSection == EPdInfoUiSection::Pandora);
	BeginMapUiContentPreload();
	RefreshMapButtonEnabledState();
	EnsureTotalMapWidget();
	HideMapOverlayImmediately();


}

void UInfoWidget::ApplyInfoLayout()
{
	const float RootWidth = FMath::Max(DesignResolution.X, 1.0f);
	const float RootHeight = FMath::Max(DesignResolution.Y, 1.0f);
	const float CenterMinWidth = FMath::Max(MinCenterPreviewWidth, 0.0f);
	const float ReservedBottomHeight = FMath::Clamp(BottomNavigationReservedHeight, 0.0f, RootHeight);

	float EffectiveLeftWidth = FMath::Max(LeftPanelWidth, 0.0f);
	float EffectiveRightWidth = FMath::Max(RightPanelWidth, 0.0f);
	const float RequestedSideWidth = EffectiveLeftWidth + EffectiveRightWidth;
	const float AvailableSideWidth = FMath::Max(RootWidth - CenterMinWidth, 0.0f);
	if (RequestedSideWidth > AvailableSideWidth && RequestedSideWidth > 0.0f)
	{
		const float Scale = AvailableSideWidth / RequestedSideWidth;
		EffectiveLeftWidth *= Scale;
		EffectiveRightWidth *= Scale;
	}

	if (DesignRootSizeBox)
	{
		DesignRootSizeBox->SetWidthOverride(RootWidth);
		DesignRootSizeBox->SetHeightOverride(RootHeight);
	}

	if (UCanvasPanelSlot* LeftSlot = LeftCurtainPanel ? UWidgetLayoutLibrary::SlotAsCanvasSlot(LeftCurtainPanel) : nullptr)
	{
		LeftSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 1.0f));
		LeftSlot->SetAlignment(FVector2D::ZeroVector);
		LeftSlot->SetOffsets(FMargin(0.0f, 0.0f, EffectiveLeftWidth, 0.0f));
	}

	if (UCanvasPanelSlot* CenterSlot = CenterPreviewPanel ? UWidgetLayoutLibrary::SlotAsCanvasSlot(CenterPreviewPanel) : nullptr)
	{
		CenterSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		CenterSlot->SetAlignment(FVector2D::ZeroVector);
		CenterSlot->SetOffsets(FMargin(EffectiveLeftWidth, 0.0f, EffectiveRightWidth, ReservedBottomHeight));
	}

	if (UCanvasPanelSlot* RightSlot = RightCurtainPanel ? UWidgetLayoutLibrary::SlotAsCanvasSlot(RightCurtainPanel) : nullptr)
	{
		RightSlot->SetAnchors(FAnchors(1.0f, 0.0f, 1.0f, 1.0f));
		RightSlot->SetAlignment(FVector2D::ZeroVector);
		RightSlot->SetOffsets(FMargin(-EffectiveRightWidth, 0.0f, EffectiveRightWidth, 0.0f));
	}

	if (UCanvasPanelSlot* BottomSlot = BottomTabBar ? UWidgetLayoutLibrary::SlotAsCanvasSlot(BottomTabBar) : nullptr)
	{
		BottomSlot->SetAnchors(FAnchors(0.5f, 1.0f, 0.5f, 1.0f));
		BottomSlot->SetAlignment(FVector2D(0.5f, 1.0f));
		BottomSlot->SetPosition(FVector2D(0.0f, -BottomTabBarBottomPadding));
		BottomSlot->SetSize(FVector2D(BottomTabBarWidth, BottomTabBarHeight));
	}
}

void UInfoWidget::NativeDestruct()
{
	ReleaseMapUiContentPreloads();

	if (bReturnCameraOnHide)
	{
		ReturnCameraToPawn();
	}
	DestroyCharacterPreview();
	HideDetailWidgets();
	if (ItemDetailWidget)
	{
		ItemDetailWidget->RemoveFromParent();
		ItemDetailWidget = nullptr;
	}
	if (PandoraDescriptionWidget)
	{
		PandoraDescriptionWidget->RemoveFromParent();
		PandoraDescriptionWidget = nullptr;
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
#if !UE_BUILD_SHIPPING
		Btn_Debug->OnClicked.RemoveDynamic(this, &ThisClass::OnDebugButtonClicked);
#endif
	}

	UnbindLeftSkinPaintCanvasEvents();
	SetCanvasExportButtonVisible(false);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MapSlideTimerHandle);
	}

	Super::NativeDestruct();
}

void UInfoWidget::BeginMapUiContentPreload()
{
	ReleaseMapUiContentPreloads();
	const int32 PreloadGeneration = ++MapUiContentPreloadGeneration;
	bMapUiContentReady = false;

	const UGameInstance* GameInstance = GetGameInstance();
	UContentDataSubsystem* ContentSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
	if (!ContentSubsystem)
	{
		bMapUiContentReady = true;
		return;
	}

	const TSoftObjectPtr<UMatchRuleDefinition>& DefaultMatchRules =
		GetDefaultMapUiMatchRuleDefinition();
	if (DefaultMatchRules.IsNull() || DefaultMatchRules.Get())
	{
		BeginMapWidgetClassPreload(PreloadGeneration);
		return;
	}

	MapRulePreloadHandle =
		ContentSubsystem->PreloadSoftObjectPathsAsync(
			{DefaultMatchRules.ToSoftObjectPath()},
			FSimpleDelegate::CreateWeakLambda(
				this,
				[this, PreloadGeneration]()
				{
					BeginMapWidgetClassPreload(PreloadGeneration);
				}));
}

void UInfoWidget::BeginMapWidgetClassPreload(const int32 PreloadGeneration)
{
	if (PreloadGeneration != MapUiContentPreloadGeneration)
	{
		return;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	UContentDataSubsystem* ContentSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
	if (!ContentSubsystem)
	{
		bMapUiContentReady = true;
		RefreshMapButtonEnabledState();
		EnsureTotalMapWidget();
		return;
	}

	TArray<FSoftObjectPath> WidgetClassPaths;
	if (const UMatchRuleDefinition* MatchRules = ResolveLoadedMapUiMatchRuleDefinition())
	{
		for (const FLobbyMatchMapOption& MapOption : MatchRules->LobbyMapOptions)
		{
			if (!MapOption.GameplayMapWidgetClass.IsNull())
			{
				WidgetClassPaths.Add(MapOption.GameplayMapWidgetClass.ToSoftObjectPath());
			}
		}
	}

	MapWidgetClassPreloadHandle =
		ContentSubsystem->PreloadSoftObjectPathsAsync(
			WidgetClassPaths,
			FSimpleDelegate::CreateWeakLambda(
				this,
				[this, PreloadGeneration]()
				{
					if (PreloadGeneration != MapUiContentPreloadGeneration)
					{
						return;
					}

					bMapUiContentReady = true;
					RefreshMapButtonEnabledState();
					EnsureTotalMapWidget();
				}));
}

void UInfoWidget::ReleaseMapUiContentPreloads()
{
	++MapUiContentPreloadGeneration;

	auto ReleaseHandle = [](TSharedPtr<FStreamableHandle>& Handle)
	{
		if (Handle.IsValid())
		{
			Handle->CancelHandle();
			Handle->ReleaseHandle();
			Handle.Reset();
		}
	};

	ReleaseHandle(MapWidgetClassPreloadHandle);
	ReleaseHandle(MapRulePreloadHandle);
	bMapUiContentReady = false;
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

void UInfoWidget::ShowItemDetailAtWidget(UItemInstance* ItemInstance, UWidget* AnchorWidget, const bool bPlaceLeftOfWidget)
{
	if (!ItemInstance || !AnchorWidget)
	{
		HideDetailWidgets();
		return;
	}

	UItemDetailWidget* DetailWidget = GetOrCreateItemDetailWidget();
	if (!DetailWidget)
	{
		return;
	}

	if (PandoraDescriptionWidget)
	{
		PandoraDescriptionWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	ActivePandoraDescriptionAnchor.Reset();
	ActivePandoraDescriptionInstance.Reset();

	DetailWidget->SetItem(ItemInstance, ResolveEquippedItemForComparison(ItemInstance));
	DetailWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PositionDetailWidgetAdjacentToWidget(DetailWidget, AnchorWidget, bPlaceLeftOfWidget);
}

void UInfoWidget::ShowSkinDetailAtWidget(USkinInstance* SkinInstance, UWidget* AnchorWidget, const bool bPlaceLeftOfWidget)
{
	if (!SkinInstance || !AnchorWidget)
	{
		HideDetailWidgets();
		return;
	}

	UItemDetailWidget* DetailWidget = GetOrCreateItemDetailWidget();
	if (!DetailWidget)
	{
		return;
	}

	if (PandoraDescriptionWidget)
	{
		PandoraDescriptionWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	ActivePandoraDescriptionAnchor.Reset();
	ActivePandoraDescriptionInstance.Reset();

	DetailWidget->SetSkin(SkinInstance);
	DetailWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PositionDetailWidgetAdjacentToWidget(DetailWidget, AnchorWidget, bPlaceLeftOfWidget);
}

void UInfoWidget::ShowSkinDefinitionDetailAtWidget(const USkinDefinition* SkinDefinition, UWidget* AnchorWidget, const bool bPlaceLeftOfWidget)
{
	if (!SkinDefinition || !AnchorWidget)
	{
		HideDetailWidgets();
		return;
	}

	UItemDetailWidget* DetailWidget = GetOrCreateItemDetailWidget();
	if (!DetailWidget)
	{
		return;
	}

	if (PandoraDescriptionWidget)
	{
		PandoraDescriptionWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	ActivePandoraDescriptionAnchor.Reset();
	ActivePandoraDescriptionInstance.Reset();

	DetailWidget->SetSkinDefinition(SkinDefinition);
	DetailWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PositionDetailWidgetAdjacentToWidget(DetailWidget, AnchorWidget, bPlaceLeftOfWidget);
}

void UInfoWidget::ShowPandoraDescriptionDetailAtWidget(UPandoraInstance* PandoraInstance, UWidget* AnchorWidget, const bool bPlaceLeftOfWidget)
{
	ShowPandoraDescriptionDetailAtWidgetInternal(PandoraInstance, AnchorWidget, bPlaceLeftOfWidget, true);
}

void UInfoWidget::ShowPandoraDescriptionDetailImmediatelyAtWidget(
	UPandoraInstance* PandoraInstance,
	UWidget* AnchorWidget,
	const bool bPlaceLeftOfWidget)
{
	ShowPandoraDescriptionDetailAtWidgetInternal(PandoraInstance, AnchorWidget, bPlaceLeftOfWidget, false);
}

void UInfoWidget::ShowPandoraDescriptionDetailAtWidgetInternal(
	UPandoraInstance* PandoraInstance,
	UWidget* AnchorWidget,
	const bool bPlaceLeftOfWidget,
	const bool bPlayShowAnimation)
{
	const APlayerController* OwningPlayer = GetOwningPlayer();
	if (!OwningPlayer || !OwningPlayer->IsLocalController())
	{
		return;
	}

	if (!PandoraInstance || !AnchorWidget)
	{
		HideDetailWidgets();
		return;
	}

	UPandoraDescriptionWidget* DetailWidget = GetOrCreatePandoraDescriptionWidget();
	if (!DetailWidget)
	{
		return;
	}

	if (ActivePandoraDescriptionAnchor.Get() == AnchorWidget
		&& ActivePandoraDescriptionInstance.Get() == PandoraInstance
		&& DetailWidget->GetVisibility() != ESlateVisibility::Collapsed)
	{
		return;
	}

	if (ItemDetailWidget)
	{
		ItemDetailWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	ActivePandoraDescriptionAnchor = AnchorWidget;
	ActivePandoraDescriptionInstance = PandoraInstance;
	UPandoraDefinition* PandoraDefinition = const_cast<UPandoraDefinition*>(PandoraInstance->PandoraDefinition.Get());
	DetailWidget->SetPandoraDefinition(PandoraDefinition);
	DetailWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	PositionDetailWidgetAdjacentToWidget(DetailWidget, AnchorWidget, bPlaceLeftOfWidget);
	if (bPlayShowAnimation)
	{
		DetailWidget->PlayShowAnimation();
	}
	else
	{
		DetailWidget->ShowWithoutAnimation();
	}
}

void UInfoWidget::HideDetailWidgets()
{
	if (ItemDetailWidget)
	{
		ItemDetailWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (PandoraDescriptionWidget)
	{
		PandoraDescriptionWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	ActivePandoraDescriptionAnchor.Reset();
	ActivePandoraDescriptionInstance.Reset();
}

void UInfoWidget::HidePandoraDescriptionDetailAtWidget(const UWidget* AnchorWidget)
{
	if (AnchorWidget && ActivePandoraDescriptionAnchor.Get() != AnchorWidget)
	{
		return;
	}

	if (PandoraDescriptionWidget)
	{
		PandoraDescriptionWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	ActivePandoraDescriptionAnchor.Reset();
	ActivePandoraDescriptionInstance.Reset();
}

void UInfoWidget::SelectProfileTab()
{
	FocusedSection = EPdInfoUiSection::Profile;
	SetPandoraUpgradeButtonVisible(false);
	PlayMapSlideOutAnimation();
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
	FocusedSection = EPdInfoUiSection::Item;
	SetPandoraUpgradeButtonVisible(false);
	PlayMapSlideOutAnimation();
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
	FocusedSection = EPdInfoUiSection::Skin;
	SetPandoraUpgradeButtonVisible(false);
	PlayMapSlideOutAnimation();
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
	FocusedSection = EPdInfoUiSection::Pandora;
	SetPandoraUpgradeButtonVisible(true);
	PlayMapSlideOutAnimation();
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
	if (IsMapButtonDisabledForCurrentMap())
	{
		HideMapOverlayImmediately();
		RefreshMapButtonEnabledState();
		return;
	}

	FocusedSection = EPdInfoUiSection::Map;
	SetPandoraUpgradeButtonVisible(false);
	HideDetailWidgets();
	HideSkinPaintCanvasGroup();
	PlaySidePanelsSlideOutAnimation();
	PlayMapSlideInAnimation();
	OnClickedMapButton.Broadcast();
}

void UInfoWidget::FocusSection(
	const EPdInfoUiSection Section,
	const bool bAnimateTransition)
{
	switch (Section)
	{
	case EPdInfoUiSection::Profile:
		SelectProfileTab();
		break;
	case EPdInfoUiSection::Item:
		SelectItemTab();
		break;
	case EPdInfoUiSection::Skin:
		SelectSkinTab();
		break;
	case EPdInfoUiSection::Pandora:
		SelectPandoraTab();
		break;
	case EPdInfoUiSection::Map:
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
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MapSlideTimerHandle);
	}

	SetIsFocusable(true);
	SetVisibility(ESlateVisibility::Visible);
	SetFocus();
	RefreshMapButtonEnabledState();
	EnsureTotalMapWidget();
	HideMapOverlayImmediately();

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



	SpawnCharacterPreview();
}

void UInfoWidget::HideInfoUi()
{
	StopAllAnimations();
	HideSkinPaintCanvasGroup();
	if (bReturnCameraOnHide)
	{
		ReturnCameraToPawn();
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

	const bool bShouldCloseMap = bMapOverlayOpen || (MapOverlay && MapOverlay->GetVisibility() != ESlateVisibility::Collapsed);
	if (bShouldCloseMap)
	{
		PlayMapSlideOutAnimation();
		++PlayedAnimationCount;
	}


}

float UInfoWidget::GetHideAnimationDelay() const
{
	const bool bShouldWaitMap = bMapOverlayOpen || (MapOverlay && MapOverlay->GetVisibility() != ESlateVisibility::Collapsed);
	const bool bHasAnyHideAnimation = SlideInLeft || SlideInRight || SlideInBottom || bShouldWaitMap;
	if (!bHasAnyHideAnimation)
	{
		return 0.0f;
	}

	const float MapDelay = bShouldWaitMap ? GetAnimationDuration(SlideMap, MapSlideDuration) : 0.0f;
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
	PlayMapSlideOutAnimation();
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
			Hud->OpenPandoraTreeUi();
		}
	}
}

void UInfoWidget::OnCanvasExportButtonClicked()
{
	APdPlayer* PlayerCharacter = Cast<APdPlayer>(GetOwningPlayerPawn());
	if (!PlayerCharacter)
	{
		SetCanvasExportButtonVisible(false);

		return;
	}

	FTransform PaintCanvasExportTransformOffset = FSkinWidgetSettings().PaintCanvasExportTransformOffset;
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		const APdHUD* PdHUD = Cast<APdHUD>(PlayerController->GetHUD());
		if (const UWidgetClassDefinition* WidgetDefinition = PdHUD ? PdHUD->GetWidgetClassDefinition() : nullptr)
		{
			PaintCanvasExportTransformOffset = WidgetDefinition->GetSkinWidgetSettings().PaintCanvasExportTransformOffset;
		}
	}

	const bool bExported = PlayerCharacter->ExportActivePaintCanvasAboveCharacterWithTransformOffset(PaintCanvasExportTransformOffset);
	SetCanvasExportButtonVisible(!bExported && PlayerCharacter->HasActivePaintCanvas());

}

void UInfoWidget::OnFaceDecalButtonClicked()
{
	APdPlayer* PlayerCharacter = Cast<APdPlayer>(GetOwningPlayerPawn());
	if (!PlayerCharacter)
	{
		SetCanvasExportButtonVisible(false);

		return;
	}

	FSkinWidgetSettings SkinSettings;
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		const APdHUD* PdHUD = Cast<APdHUD>(PlayerController->GetHUD());
		if (const UWidgetClassDefinition* WidgetDefinition = PdHUD ? PdHUD->GetWidgetClassDefinition() : nullptr)
		{
			SkinSettings = WidgetDefinition->GetSkinWidgetSettings();
		}
	}

	UMaterialInterface* FaceDecalMaterial = SkinSettings.PaintCanvasFaceDecalMaterial.Get();
	if (!FaceDecalMaterial)
	{

		return;
	}

	const bool bApplied = PlayerCharacter->ApplyActivePaintCanvasToFaceDecal(
		FaceDecalMaterial,
		SkinSettings.PaintCanvasFaceDecalSocketName,
		SkinSettings.PaintCanvasFaceDecalTransformOffset,
		SkinSettings.PaintCanvasFaceDecalSize,
		SkinSettings.PaintCanvasFaceDecalTextureParameterName);

	SetCanvasExportButtonVisible(PlayerCharacter->HasActivePaintCanvas());

}

void UInfoWidget::OnDebugButtonClicked()
{
	if (Btn_Debug)
	{
		Btn_Debug->SetIsEnabled(false);
	}

	if (APdPlayerController* PlayerController = Cast<APdPlayerController>(GetOwningPlayer()))
	{
		PlayerController->RequestDebugGrantTestResources();
	}

	if (Btn_Debug)
	{
		Btn_Debug->SetVisibility(ESlateVisibility::Collapsed);
	}
}

bool UInfoWidget::IsMapButtonDisabledForCurrentMap() const
{
	if (MapButtonDisabledMapNames.IsEmpty())
	{
		return false;
	}

	const FString CurrentLevelName = UGameplayStatics::GetCurrentLevelName(this, true);
	if (CurrentLevelName.IsEmpty())
	{
		return false;
	}

	for (const FName DisabledMapName : MapButtonDisabledMapNames)
	{
		if (DisabledMapName.IsNone())
		{
			continue;
		}

		if (CurrentLevelName.Equals(DisabledMapName.ToString(), ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	return false;
}

void UInfoWidget::ApplyWidgetDefinitionSettings()
{
	if (const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this))
	{
		const FInfoWidgetSettings& Settings = WidgetDefinition->GetInfoWidgetSettings();
		bAutoApplyInfoLayout = Settings.bAutoApplyInfoLayout;
		DesignResolution = Settings.DesignResolution;
		LeftPanelWidth = Settings.LeftPanelWidth;
		RightPanelWidth = Settings.RightPanelWidth;
		MinCenterPreviewWidth = Settings.MinCenterPreviewWidth;
		BottomNavigationReservedHeight = Settings.BottomNavigationReservedHeight;
		BottomTabBarWidth = Settings.BottomTabBarWidth;
		BottomTabBarHeight = Settings.BottomTabBarHeight;
		BottomTabBarBottomPadding = Settings.BottomTabBarBottomPadding;
		HideAnimationDelay = Settings.HideAnimationDelay;
		MapSlideStartOffset = Settings.MapSlideStartOffset;
		MapSlideDuration = Settings.MapSlideDuration;
		bUseCharacterPreviewCamera = Settings.bUseCharacterPreviewCamera;
		bReturnCameraOnHide = Settings.bReturnCameraOnHide;
		DetailPopupOffset = Settings.DetailPopupOffset;
		MapButtonDisabledMapNames = Settings.MapButtonDisabledMapNames;

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

void UInfoWidget::RefreshMapButtonEnabledState()
{
	if (!MapButton)
	{
		return;
	}

	const bool bDisableMapButton = !bMapUiContentReady || IsMapButtonDisabledForCurrentMap();
	MapButton->SetIsEnabled(!bDisableMapButton);

	if (bDisableMapButton)
	{
		HideMapOverlayImmediately();
	}
}

bool UInfoWidget::EnsureMapOverlay()
{
	if (MapOverlay)
	{
		return true;
	}

	if (!WidgetTree)
	{

		return false;
	}

	UOverlay* NewMapOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("MapOverlay"));
	if (!NewMapOverlay)
	{

		return false;
	}

	UWidget* RootWidget = WidgetTree->RootWidget;
	if (UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(RootWidget))
	{
		UCanvasPanelSlot* OverlaySlot = RootCanvas->AddChildToCanvas(NewMapOverlay);
		if (OverlaySlot)
		{
			OverlaySlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			OverlaySlot->SetOffsets(FMargin(0.0f));
			OverlaySlot->SetAlignment(FVector2D::ZeroVector);
			OverlaySlot->SetZOrder(10);
		}
	}
	else if (UPanelWidget* RootPanel = Cast<UPanelWidget>(RootWidget))
	{
		RootPanel->AddChild(NewMapOverlay);
	}
	else
	{

		return false;
	}

	NewMapOverlay->SetVisibility(ESlateVisibility::Collapsed);
	MapOverlay = NewMapOverlay;


	return true;
}

TSubclassOf<UMapWidget> UInfoWidget::ResolveTotalMapWidgetClassForCurrentMap() const
{
	FLobbyMatchMapOption MapOption;
	if (FindMapOptionForCurrentMapUi(this, MapOption))
	{
		UClass* LoadedWidgetClass = MapOption.GameplayMapWidgetClass.Get();
		if (LoadedWidgetClass && LoadedWidgetClass->IsChildOf(UMapWidget::StaticClass()))
		{

			return LoadedWidgetClass;
		}

		if (!MapOption.GameplayMapWidgetClass.IsNull())
		{
			return nullptr;
		}
	}

	if (TotalMapWidgetClass)
	{
		return TotalMapWidgetClass;
	}

	if (const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this))
	{
		return WidgetDefinition->GetTotalMapWidgetClass();
	}

	return nullptr;
}

void UInfoWidget::EnsureTotalMapWidget()
{
	if (TotalMap)
	{
		return;
	}

	if (!bMapUiContentReady)
	{
		return;
	}

	if (!EnsureMapOverlay())
	{
		return;
	}

	TSubclassOf<UMapWidget> ResolvedMapWidgetClass = ResolveTotalMapWidgetClassForCurrentMap();
	if (!ResolvedMapWidgetClass)
	{

		return;
	}

	TotalMap = CreateWidget<UMapWidget>(GetOwningPlayer(), ResolvedMapWidgetClass);
	if (!TotalMap)
	{

		return;
	}

	TotalMap->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (UOverlaySlot* MapSlot = MapOverlay->AddChildToOverlay(TotalMap))
	{
		MapSlot->SetHorizontalAlignment(HAlign_Fill);
		MapSlot->SetVerticalAlignment(VAlign_Fill);
	}


}

void UInfoWidget::HideMapOverlayImmediately()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MapSlideTimerHandle);
	}

	bMapOverlayOpen = false;
	bMapSlideReverse = false;

	if (MapOverlay)
	{
		MapOverlay->SetVisibility(ESlateVisibility::Collapsed);
		MapOverlay->SetRenderTranslation(MapSlideStartOffset);
		MapOverlay->SetRenderOpacity(0.0f);
	}
}

void UInfoWidget::PlayMapSlideInAnimation()
{
	EnsureTotalMapWidget();

	if (!MapOverlay)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MapSlideTimerHandle);
	}

	bMapOverlayOpen = true;
	bMapSlideReverse = false;
	MapOverlay->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	if (SlideMap)
	{
		StopAnimation(SlideMap);
		MapOverlay->SetRenderTranslation(FVector2D::ZeroVector);
		MapOverlay->SetRenderOpacity(1.0f);
		UnbindAllFromAnimationFinished(SlideMap);
		PlayAnimation(SlideMap, 0.0f, 1, EUMGSequencePlayMode::Forward, 1.0f, false);
		return;
	}

	if (UWorld* World = GetWorld())
	{
		MapSlideStartTime = World->GetTimeSeconds();
		MapOverlay->SetRenderTranslation(MapSlideStartOffset);
		MapOverlay->SetRenderOpacity(0.0f);
		World->GetTimerManager().SetTimer(
			MapSlideTimerHandle,
			this,
			&ThisClass::TickMapSlideAnimation,
			1.0f / 60.0f,
			true);
	}
}

void UInfoWidget::PlayMapSlideOutAnimation()
{
	if (!MapOverlay || (!bMapOverlayOpen && MapOverlay->GetVisibility() == ESlateVisibility::Collapsed))
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MapSlideTimerHandle);
	}

	bMapOverlayOpen = false;
	bMapSlideReverse = true;
	MapOverlay->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	if (SlideMap)
	{
		StopAnimation(SlideMap);
		UnbindAllFromAnimationFinished(SlideMap);
		FWidgetAnimationDynamicEvent FinishedEvent;
		FinishedEvent.BindDynamic(this, &ThisClass::HandleMapSlideAnimationFinished);
		BindToAnimationFinished(SlideMap, FinishedEvent);
		PlayAnimationReverse(SlideMap, 1.0f, false);

		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				MapSlideTimerHandle,
				this,
				&ThisClass::FinishMapSlideOutAnimation,
				GetAnimationDuration(SlideMap, MapSlideDuration),
				false);
		}
		return;
	}

	if (UWorld* World = GetWorld())
	{
		MapSlideStartTime = World->GetTimeSeconds();
		MapOverlay->SetRenderTranslation(FVector2D::ZeroVector);
		MapOverlay->SetRenderOpacity(1.0f);
		World->GetTimerManager().SetTimer(
			MapSlideTimerHandle,
			this,
			&ThisClass::TickMapSlideAnimation,
			1.0f / 60.0f,
			true);
	}
}

void UInfoWidget::TickMapSlideAnimation()
{
	UWorld* World = GetWorld();
	if (!World || !MapOverlay)
	{
		return;
	}

	const float Alpha = MapSlideDuration > 0.0f
		? FMath::Clamp(static_cast<float>((World->GetTimeSeconds() - MapSlideStartTime) / MapSlideDuration), 0.0f, 1.0f)
		: 1.0f;
	const float EaseAlpha = FMath::InterpEaseOut(0.0f, 1.0f, Alpha, 3.0f);

	if (bMapSlideReverse)
	{
		MapOverlay->SetRenderTranslation(FMath::Lerp(FVector2D::ZeroVector, MapSlideStartOffset, EaseAlpha));
		MapOverlay->SetRenderOpacity(1.0f - EaseAlpha);
	}
	else
	{
		MapOverlay->SetRenderTranslation(FMath::Lerp(MapSlideStartOffset, FVector2D::ZeroVector, EaseAlpha));
		MapOverlay->SetRenderOpacity(EaseAlpha);
	}

	if (Alpha >= 1.0f)
	{
		World->GetTimerManager().ClearTimer(MapSlideTimerHandle);

		if (bMapSlideReverse)
		{
			FinishMapSlideOutAnimation();
		}
		else
		{
			MapOverlay->SetRenderTranslation(FVector2D::ZeroVector);
			MapOverlay->SetRenderOpacity(1.0f);
		}
	}
}

void UInfoWidget::FinishMapSlideOutAnimation()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MapSlideTimerHandle);
	}

	if (SlideMap)
	{
		UnbindAllFromAnimationFinished(SlideMap);
	}

	if (MapOverlay)
	{
		MapOverlay->SetVisibility(ESlateVisibility::Collapsed);
		MapOverlay->SetRenderTranslation(MapSlideStartOffset);
		MapOverlay->SetRenderOpacity(0.0f);
	}

	bMapOverlayOpen = false;
	bMapSlideReverse = false;
}

void UInfoWidget::HandleMapSlideAnimationFinished()
{
	if (bMapSlideReverse)
	{
		FinishMapSlideOutAnimation();
		return;
	}

	if (MapOverlay)
	{
		MapOverlay->SetRenderTranslation(FVector2D::ZeroVector);
		MapOverlay->SetRenderOpacity(1.0f);
	}
}

void UInfoWidget::HandlePaintCanvasGroupVisibilityChanged(const bool bVisible)
{
	SetCanvasExportButtonVisible(bVisible);
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

void UInfoWidget::ResolveCharacterPreviewClass()
{
	if (CharacterPreviewClass)
	{
		return;
	}

	if (const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this))
	{
		CharacterPreviewClass = WidgetDefinition->GetCharacterPreviewClass();
	}
}

void UInfoWidget::SpawnCharacterPreview()
{
	if (!bUseCharacterPreviewCamera)
	{
		return;
	}

	APawn* OwningPawn = GetOwningPlayerPawn();
	USkeletalMeshComponent* MeshComponent = OwningPawn
		? OwningPawn->FindComponentByClass<USkeletalMeshComponent>()
		: nullptr;

	if (IsValid(SpawnedCharacterPreview))
	{
		if (!MeshComponent)
		{

			return;
		}

		DisablePreviewCameraLetterboxing(SpawnedCharacterPreview.Get());

		if (APlayerController* PlayerController = GetOwningPlayer())
		{
			CutToViewTarget(PlayerController, SpawnedCharacterPreview.Get());
		}
		return;
	}

	ResolveCharacterPreviewClass();

	UWorld* World = GetWorld();
	if (!World || !CharacterPreviewClass || !MeshComponent)
	{

		return;
	}

	SpawnedCharacterPreview = World->SpawnActor<AActor>(CharacterPreviewClass, FTransform::Identity);
	if (!SpawnedCharacterPreview)
	{

		return;
	}

	SpawnedCharacterPreview->AttachToComponent(
		MeshComponent,
		FAttachmentTransformRules(
			EAttachmentRule::KeepRelative,
			EAttachmentRule::KeepRelative,
			EAttachmentRule::KeepRelative,
			true));

	DisablePreviewCameraLetterboxing(SpawnedCharacterPreview.Get());

	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		CutToViewTarget(PlayerController, SpawnedCharacterPreview.Get());
	}
}

void UInfoWidget::ReturnCameraToPawn() const
{
	APlayerController* PlayerController = GetOwningPlayer();
	APawn* OwningPawn = GetOwningPlayerPawn();
	if (PlayerController && OwningPawn)
	{
		CutToViewTarget(PlayerController, OwningPawn);
	}
}

void UInfoWidget::DestroyCharacterPreview()
{
	if (IsValid(SpawnedCharacterPreview))
	{
		SpawnedCharacterPreview->Destroy();
	}

	SpawnedCharacterPreview = nullptr;
}

UItemDetailWidget* UInfoWidget::GetOrCreateItemDetailWidget()
{
	if (ItemDetailWidget)
	{
		return ItemDetailWidget.Get();
	}

	if (!ItemDetailWidgetClass)
	{

		return nullptr;
	}

	ItemDetailWidget = CreateWidget<UItemDetailWidget>(GetOwningPlayer(), ItemDetailWidgetClass);
	if (ItemDetailWidget)
	{
		ItemDetailWidget->AddToViewport(100);
		ItemDetailWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	return ItemDetailWidget.Get();
}

UPandoraDescriptionWidget* UInfoWidget::GetOrCreatePandoraDescriptionWidget()
{
	if (PandoraDescriptionWidget)
	{
		return PandoraDescriptionWidget.Get();
	}

	if (!PandoraDescriptionWidgetClass)
	{
		if (const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this))
		{
			PandoraDescriptionWidgetClass = WidgetDefinition->GetPandoraDescriptionWidgetClass();
		}
	}

	if (!PandoraDescriptionWidgetClass)
	{
		return nullptr;
	}

	PandoraDescriptionWidget = CreateWidget<UPandoraDescriptionWidget>(GetOwningPlayer(), PandoraDescriptionWidgetClass);
	if (PandoraDescriptionWidget)
	{
		PandoraDescriptionWidget->AddToViewport(100);
		PandoraDescriptionWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	return PandoraDescriptionWidget.Get();
}

void UInfoWidget::PositionDetailWidgetAdjacentToWidget(UUserWidget* DetailWidget, const UWidget* AnchorWidget, const bool bPlaceLeftOfWidget) const
{
	if (!DetailWidget || !AnchorWidget)
	{
		return;
	}

	const FGeometry& AnchorGeometry = AnchorWidget->GetCachedGeometry();
	FVector2D PixelPosition;
	FVector2D ViewportPosition;
	USlateBlueprintLibrary::LocalToViewport(
		this,
		AnchorGeometry,
		bPlaceLeftOfWidget ? FVector2D::ZeroVector : FVector2D(AnchorGeometry.GetLocalSize().X, 0.0f),
		PixelPosition,
		ViewportPosition);

	DetailWidget->ForceLayoutPrepass();
	const FVector2D DesiredSize = DetailWidget->GetDesiredSize();
	const FVector2D ViewportSize = UWidgetLayoutLibrary::GetViewportSize(this);

	FVector2D PopupPosition = bPlaceLeftOfWidget
		? FVector2D(ViewportPosition.X - DesiredSize.X - DetailPopupOffset.X, ViewportPosition.Y + DetailPopupOffset.Y)
		: FVector2D(ViewportPosition.X + DetailPopupOffset.X, ViewportPosition.Y + DetailPopupOffset.Y);

	if (ViewportSize.X > 0.0f && DesiredSize.X > 0.0f)
	{
		PopupPosition.X = FMath::Clamp(PopupPosition.X, 0.0f, FMath::Max(ViewportSize.X - DesiredSize.X, 0.0f));
	}

	if (ViewportSize.Y > 0.0f && DesiredSize.Y > 0.0f)
	{
		PopupPosition.Y = FMath::Clamp(PopupPosition.Y, 0.0f, FMath::Max(ViewportSize.Y - DesiredSize.Y, 0.0f));
	}

	DetailWidget->SetPositionInViewport(PopupPosition, false);
}

UItemInstance* UInfoWidget::ResolveEquippedItemForComparison(UItemInstance* HoveredItem) const
{
	if (!HoveredItem || !WB_LeftEquipment)
	{
		return nullptr;
	}

	const UEquipSlotWidget* EquippedSlot = WB_LeftEquipment->FindFirstEquippedCompatibleEquipSlot(HoveredItem);
	UItemInstance* EquippedItem = EquippedSlot ? EquippedSlot->GetItemInstance() : nullptr;
	return EquippedItem != HoveredItem ? EquippedItem : nullptr;
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

	SetCanvasExportButtonVisible(false);
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
	const bool bInside =
		LocalPosition.X >= 0.0f &&
		LocalPosition.Y >= 0.0f &&
		LocalPosition.X <= LocalSize.X &&
		LocalPosition.Y <= LocalSize.Y;



	return bInside;
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
