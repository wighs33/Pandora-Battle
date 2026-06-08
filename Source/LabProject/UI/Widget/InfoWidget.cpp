#include "UI/Widget/InfoWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Camera/CameraComponent.h"
#include "Common/ProjectTagConfig.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SizeBox.h"
#include "Components/WidgetSwitcher.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Item/ItemInstance.h"
#include "Pandora/PandoraDefinition.h"
#include "Pandora/PandoraInstance.h"
#include "Skin/SkinInstance.h"
#include "UI/Widget/EquipSlotWidget.h"
#include "UI/Widget/ItemDetailWidget.h"
#include "UI/Widget/ItemSlotDragDropOperation.h"
#include "UI/Widget/PandoraDescriptionWidget.h"
#include "UI/Widget/SkinSlotDragDropOperation.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InfoWidget)

DEFINE_LOG_CATEGORY_STATIC(LogInfoWidget, Log, All);

namespace
{
	void DisablePreviewCameraLetterboxing(AActor* ViewTarget, const TCHAR* LogContext)
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

		if (!CameraComponents.IsEmpty())
		{
			UE_LOG(LogInfoWidget, Log,
				TEXT("[%s] Disabled preview camera aspect constraint. viewTarget=%s cameraCount=%d"),
				LogContext ? LogContext : TEXT("PreviewCamera"),
				*GetNameSafe(ViewTarget),
				CameraComponents.Num());
		}
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

	if (bAutoApplyInfoLayout)
	{
		ApplyInfoLayout();
	}
}

void UInfoWidget::NativeConstruct()
{
	Super::NativeConstruct();

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

	UE_LOG(LogInfoWidget, Log,
		TEXT("[InfoAnimation] Construct widget=%s SlideInLeft=%s SlideInRight=%s SlideInBottom=%s"),
		*GetNameSafe(this),
		*GetNameSafe(SlideInLeft),
		*GetNameSafe(SlideInRight),
		*GetNameSafe(SlideInBottom));
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
	ReturnCameraToPawn(PreviewCameraHideBlendTime);
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
			UE_LOG(LogInfoWidget, Log, TEXT("[CharacterPanelDrop] Item dropped on character panel: widget=%s item=%s sourceSlot=%d"),
				*GetNameSafe(this),
				*GetNameSafe(DroppedItem),
				ItemDragOperation->GetSourceSlotIndex());
			OnDroppedItemToCharacterPanel.Broadcast(DroppedItem);
			return true;
		}
	}

	if (USkinSlotDragDropOperation* SkinDragOperation = Cast<USkinSlotDragDropOperation>(InOperation))
	{
		USkinInstance* DroppedSkin = SkinDragOperation->GetSkinInstance();
		if (DroppedSkin)
		{
			UE_LOG(LogInfoWidget, Log, TEXT("[CharacterPanelDrop] Skin dropped on character panel: widget=%s skin=%s sourceSlot=%d"),
				*GetNameSafe(this),
				*GetNameSafe(DroppedSkin),
				SkinDragOperation->GetSourceSlotIndex());
			OnDroppedSkinToCharacterPanel.Broadcast(DroppedSkin);
			return true;
		}
	}

	return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
}

void UInfoWidget::ShowItemDetail(UItemInstance* ItemInstance, UWidget* AnchorWidget)
{
	ShowItemDetailAtWidget(ItemInstance, AnchorWidget, false);
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

	DetailWidget->SetItem(ItemInstance, ResolveEquippedItemForComparison(ItemInstance));
	DetailWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PositionDetailWidgetAdjacentToWidget(DetailWidget, AnchorWidget, bPlaceLeftOfWidget);
}

void UInfoWidget::ShowItemDetailAtCursor(UItemInstance* ItemInstance, const FVector2D ScreenSpacePosition, const bool bPlaceLeftOfCursor)
{
	if (!ItemInstance)
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

	DetailWidget->SetItem(ItemInstance, ResolveEquippedItemForComparison(ItemInstance));
	DetailWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PositionDetailWidgetAtCursor(DetailWidget, ScreenSpacePosition, bPlaceLeftOfCursor);
}

void UInfoWidget::ShowSkinDetail(USkinInstance* SkinInstance, UWidget* AnchorWidget)
{
	ShowSkinDetailAtWidget(SkinInstance, AnchorWidget, false);
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

	DetailWidget->SetSkin(SkinInstance);
	DetailWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PositionDetailWidgetAdjacentToWidget(DetailWidget, AnchorWidget, bPlaceLeftOfWidget);
}

void UInfoWidget::ShowSkinDetailAtCursor(USkinInstance* SkinInstance, const FVector2D ScreenSpacePosition, const bool bPlaceLeftOfCursor)
{
	if (!SkinInstance)
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

	DetailWidget->SetSkin(SkinInstance);
	DetailWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PositionDetailWidgetAtCursor(DetailWidget, ScreenSpacePosition, bPlaceLeftOfCursor);
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

	DetailWidget->SetSkinDefinition(SkinDefinition);
	DetailWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PositionDetailWidgetAdjacentToWidget(DetailWidget, AnchorWidget, bPlaceLeftOfWidget);
}

void UInfoWidget::ShowSkinDefinitionDetailAtCursor(const USkinDefinition* SkinDefinition, const FVector2D ScreenSpacePosition, const bool bPlaceLeftOfCursor)
{
	if (!SkinDefinition)
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

	DetailWidget->SetSkinDefinition(SkinDefinition);
	DetailWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PositionDetailWidgetAtCursor(DetailWidget, ScreenSpacePosition, bPlaceLeftOfCursor);
}

void UInfoWidget::ShowPandoraDescriptionDetail(UPandoraInstance* PandoraInstance, UWidget* AnchorWidget)
{
	ShowPandoraDescriptionDetailAtWidget(PandoraInstance, AnchorWidget, false);
}

void UInfoWidget::ShowPandoraDescriptionDetailAtWidget(UPandoraInstance* PandoraInstance, UWidget* AnchorWidget, const bool bPlaceLeftOfWidget)
{
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

	if (ItemDetailWidget)
	{
		ItemDetailWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	UPandoraDefinition* PandoraDefinition = const_cast<UPandoraDefinition*>(PandoraInstance->PandoraDefinition.Get());
	DetailWidget->SetPandoraDefinition(PandoraDefinition);
	DetailWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PositionDetailWidgetAdjacentToWidget(DetailWidget, AnchorWidget, bPlaceLeftOfWidget);
}

void UInfoWidget::ShowPandoraDescriptionDetailAtCursor(UPandoraInstance* PandoraInstance, const FVector2D ScreenSpacePosition, const bool bPlaceLeftOfCursor)
{
	if (!PandoraInstance)
	{
		HideDetailWidgets();
		return;
	}

	UPandoraDescriptionWidget* DetailWidget = GetOrCreatePandoraDescriptionWidget();
	if (!DetailWidget)
	{
		return;
	}

	if (ItemDetailWidget)
	{
		ItemDetailWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	UPandoraDefinition* PandoraDefinition = const_cast<UPandoraDefinition*>(PandoraInstance->PandoraDefinition.Get());
	DetailWidget->SetPandoraDefinition(PandoraDefinition);
	DetailWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PositionDetailWidgetAtCursor(DetailWidget, ScreenSpacePosition, bPlaceLeftOfCursor);
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
}

void UInfoWidget::SelectProfileTab()
{
	SelectInfoCenterPage(
		WB_LeftProfile,
		WB_RightStatus,
		GetProfileLeftUiTag(),
		GetProfileRightUiTag());
}

void UInfoWidget::SelectItemTab()
{
	SelectInfoCenterPage(
		WB_LeftEquipment,
		WB_RightInventory,
		GetItemLeftUiTag(),
		GetItemRightUiTag());
}

void UInfoWidget::SelectSkinTab()
{
	SelectInfoCenterPage(
		WB_LeftSkin,
		WB_RightSkin,
		GetSkinLeftUiTag(),
		GetSkinRightUiTag());
}

void UInfoWidget::SelectPandoraTab()
{
	SelectInfoCenterPage(
		WB_LeftPandora,
		WB_RightPandora,
		GetPandoraLeftUiTag(),
		GetPandoraRightUiTag());
}

void UInfoWidget::SelectTabByLeftTag(FGameplayTag LeftUiTag)
{
	if (LeftUiTag == GetItemLeftUiTag())
	{
		SelectItemTab();
		return;
	}

	if (LeftUiTag == GetSkinLeftUiTag())
	{
		SelectSkinTab();
		return;
	}

	if (LeftUiTag == GetPandoraLeftUiTag())
	{
		SelectPandoraTab();
		return;
	}

	SelectProfileTab();
}

void UInfoWidget::ShowInfoUi()
{
	StopAllAnimations();
	SetIsFocusable(true);
	SetVisibility(ESlateVisibility::Visible);
	SetFocus();

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

	UE_LOG(LogInfoWidget, Log,
		TEXT("[InfoAnimation] Show widget=%s played=%d left=%s right=%s bottom=%s"),
		*GetNameSafe(this),
		PlayedAnimationCount,
		*GetNameSafe(SlideInLeft),
		*GetNameSafe(SlideInRight),
		*GetNameSafe(SlideInBottom));

	SpawnCharacterPreview();
}

void UInfoWidget::HideInfoUi()
{
	StopAllAnimations();
	ReturnCameraToPawn(PreviewCameraHideBlendTime);

	int32 PlayedAnimationCount = 0;
	if (SlideInLeft)
	{
		PlayAnimation(SlideInLeft, 0.0f, 1, EUMGSequencePlayMode::Reverse, 1.0f, false);
		++PlayedAnimationCount;
	}

	if (SlideInRight)
	{
		PlayAnimation(SlideInRight, 0.0f, 1, EUMGSequencePlayMode::Reverse, 1.0f, false);
		++PlayedAnimationCount;
	}

	if (SlideInBottom)
	{
		PlayAnimation(SlideInBottom, 0.0f, 1, EUMGSequencePlayMode::Reverse, 1.0f, false);
		++PlayedAnimationCount;
	}

	UE_LOG(LogInfoWidget, Log,
		TEXT("[InfoAnimation] Hide widget=%s played=%d left=%s right=%s bottom=%s"),
		*GetNameSafe(this),
		PlayedAnimationCount,
		*GetNameSafe(SlideInLeft),
		*GetNameSafe(SlideInRight),
		*GetNameSafe(SlideInBottom));
}

float UInfoWidget::GetHideAnimationDelay() const
{
	return (SlideInLeft || SlideInRight || SlideInBottom) ? HideAnimationDelay : 0.0f;
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
	OnClickedMapButton.Broadcast();
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

	UE_LOG(LogInfoWidget, Log,
		TEXT("[InfoAnimation] TabSlide widget=%s played=%d left=%s right=%s"),
		*GetNameSafe(this),
		PlayedAnimationCount,
		*GetNameSafe(SlideInLeft),
		*GetNameSafe(SlideInRight));
}

void UInfoWidget::ResolveCharacterPreviewClass()
{
	if (CharacterPreviewClass)
	{
		return;
	}

	CharacterPreviewClass = LoadClass<AActor>(
		nullptr,
		TEXT("/Game/CharacterPreview/BP_CharacterPreview.BP_CharacterPreview_C"));
}

void UInfoWidget::SpawnCharacterPreview()
{
	if (!bUseCharacterPreviewCamera)
	{
		return;
	}

	if (IsValid(SpawnedCharacterPreview))
	{
		DisablePreviewCameraLetterboxing(SpawnedCharacterPreview.Get(), TEXT("InfoPreview"));

		if (APlayerController* PlayerController = GetOwningPlayer())
		{
			PlayerController->SetViewTargetWithBlend(
				SpawnedCharacterPreview.Get(),
				PreviewCameraShowBlendTime,
				VTBlend_Cubic);
		}
		return;
	}

	ResolveCharacterPreviewClass();

	APawn* OwningPawn = GetOwningPlayerPawn();
	USkeletalMeshComponent* MeshComponent = OwningPawn
		? OwningPawn->FindComponentByClass<USkeletalMeshComponent>()
		: nullptr;
	UWorld* World = GetWorld();
	if (!World || !CharacterPreviewClass || !MeshComponent)
	{
		UE_LOG(LogInfoWidget, Warning,
			TEXT("[InfoPreview] Spawn skipped. widget=%s world=%s previewClass=%s pawn=%s mesh=%s"),
			*GetNameSafe(this),
			*GetNameSafe(World),
			*GetNameSafe(CharacterPreviewClass.Get()),
			*GetNameSafe(OwningPawn),
			*GetNameSafe(MeshComponent));
		return;
	}

	SpawnedCharacterPreview = World->SpawnActor<AActor>(CharacterPreviewClass, FTransform::Identity);
	if (!SpawnedCharacterPreview)
	{
		UE_LOG(LogInfoWidget, Warning,
			TEXT("[InfoPreview] Spawn failed. widget=%s previewClass=%s"),
			*GetNameSafe(this),
			*GetNameSafe(CharacterPreviewClass.Get()));
		return;
	}

	SpawnedCharacterPreview->AttachToComponent(
		MeshComponent,
		FAttachmentTransformRules(
			EAttachmentRule::KeepRelative,
			EAttachmentRule::KeepRelative,
			EAttachmentRule::KeepRelative,
			true));

	DisablePreviewCameraLetterboxing(SpawnedCharacterPreview.Get(), TEXT("InfoPreview"));

	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		PlayerController->SetViewTargetWithBlend(
			SpawnedCharacterPreview.Get(),
			PreviewCameraShowBlendTime,
			VTBlend_Cubic);
	}

	UE_LOG(LogInfoWidget, Log,
		TEXT("[InfoPreview] Spawned widget=%s preview=%s pawn=%s mesh=%s worldLocation=%s worldRotation=%s"),
		*GetNameSafe(this),
		*GetNameSafe(SpawnedCharacterPreview.Get()),
		*GetNameSafe(OwningPawn),
		*GetNameSafe(MeshComponent),
		*SpawnedCharacterPreview->GetActorLocation().ToCompactString(),
		*SpawnedCharacterPreview->GetActorRotation().ToCompactString());
}

void UInfoWidget::ReturnCameraToPawn(float BlendTime) const
{
	APlayerController* PlayerController = GetOwningPlayer();
	APawn* OwningPawn = GetOwningPlayerPawn();
	if (PlayerController && OwningPawn)
	{
		PlayerController->SetViewTargetWithBlend(OwningPawn, BlendTime, VTBlend_Cubic);
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
		UE_LOG(LogInfoWidget, Warning, TEXT("[InfoDetail] ItemDetailWidgetClass is not set on %s"), *GetNameSafe(this));
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
		PandoraDescriptionWidgetClass = LoadClass<UPandoraDescriptionWidget>(
			nullptr,
			TEXT("/Game/UI/Widget/WBP_PandoraDescription.WBP_PandoraDescription_C"));
	}

	if (!PandoraDescriptionWidgetClass)
	{
		UE_LOG(LogInfoWidget, Warning, TEXT("[InfoDetail] PandoraDescriptionWidgetClass is not set on %s"), *GetNameSafe(this));
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

void UInfoWidget::PositionDetailWidget(UUserWidget* DetailWidget, const UWidget* AnchorWidget) const
{
	PositionDetailWidgetAdjacentToWidget(DetailWidget, AnchorWidget, false);
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

void UInfoWidget::PositionDetailWidgetAtCursor(UUserWidget* DetailWidget, const FVector2D ScreenSpacePosition, const bool bPlaceLeftOfCursor) const
{
	if (!DetailWidget)
	{
		return;
	}

	FVector2D PixelPosition;
	FVector2D ViewportPosition;
	USlateBlueprintLibrary::AbsoluteToViewport(this, ScreenSpacePosition, PixelPosition, ViewportPosition);

	DetailWidget->ForceLayoutPrepass();
	const FVector2D DesiredSize = DetailWidget->GetDesiredSize();
	const FVector2D ViewportSize = UWidgetLayoutLibrary::GetViewportSize(this);

	FVector2D PopupPosition = bPlaceLeftOfCursor
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

	UE_LOG(LogInfoWidget, Verbose, TEXT("[CharacterPanelDrop] Hit test: widget=%s panel=%s screen=%s local=%s size=%s inside=%s"),
		*GetNameSafe(this),
		*GetNameSafe(DropPanel),
		*ScreenSpacePosition.ToString(),
		*LocalPosition.ToString(),
		*LocalSize.ToString(),
		bInside ? TEXT("true") : TEXT("false"));

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
