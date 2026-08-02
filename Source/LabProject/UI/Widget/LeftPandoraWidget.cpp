#include "UI/Widget/LeftPandoraWidget.h"

#include "Common/Enum_Direction.h"
#include "Component/AbilitySystem/PandoraTreeComponent.h"
#include "Component/Pandora/PandoraComponent.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Mode/PdPlayerState.h"
#include "Pandora/PandoraInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LeftPandoraWidget)

void ULeftPandoraWidget::NativeConstruct()
{
	Super::NativeConstruct();

	CacheDefaultWeaponImageBrushes();
	RebuildPandoraEquipSlotList();
	BindPandoraEquipSlotCallbacks();
	BindPandoraLoadoutChanged();
	RefreshPandoraLoadoutSlots(BoundPandoraComponent.Get());
}

void ULeftPandoraWidget::NativeDestruct()
{
	UnbindPandoraTreeChanged();
	UnbindPandoraLoadoutChanged();
	UnbindPandoraEquipSlotCallbacks();

	Super::NativeDestruct();
}

void ULeftPandoraWidget::ToggleActiveEquipSlots(bool bActive)
{
	RebuildPandoraEquipSlotList();

	for (UPandoraEquipSlotWidget* PandoraEquipSlot : PandoraEquipSlotList)
	{
		if (PandoraEquipSlot)
		{
			PandoraEquipSlot->SetIsEnabled(bActive);
		}
	}
}

void ULeftPandoraWidget::SelectPandoraEquipSlot(UPandoraEquipSlotWidget* InSelectedPandoraEquipSlot)
{
	if (!IsValid(InSelectedPandoraEquipSlot))
	{
		return;
	}

	SelectedPandoraEquipSlot = InSelectedPandoraEquipSlot;

	const bool bWasSelectedAnyButton = bIsSelectedAnyButton;
	OnClicked_PandoraEquipSlot.Broadcast(InSelectedPandoraEquipSlot, bWasSelectedAnyButton);

	// A filled slot click can clear selection synchronously through InfoUiPresenter.
	if (SelectedPandoraEquipSlot != InSelectedPandoraEquipSlot || !IsValid(SelectedPandoraEquipSlot))
	{
		return;
	}

	ToggleActiveEquipSlots(bWasSelectedAnyButton);
	SelectedPandoraEquipSlot->SetIsEnabled(true);
	bIsSelectedAnyButton = !bWasSelectedAnyButton;
}

void ULeftPandoraWidget::ClearPandoraEquipSlotSelection()
{
	RebuildPandoraEquipSlotList();

	for (UPandoraEquipSlotWidget* PandoraEquipSlot : PandoraEquipSlotList)
	{
		if (PandoraEquipSlot)
		{
			PandoraEquipSlot->SetIsEnabled(true);
			PandoraEquipSlot->ToggleText_Apply(false);
		}
	}

	SelectedPandoraEquipSlot = nullptr;
	bIsSelectedAnyButton = false;
}

void ULeftPandoraWidget::RefreshPandoraLoadoutSlots(const UPandoraComponent* PandoraComponent)
{
	RebuildPandoraEquipSlotList();

	if (UPandoraComponent* MutablePandoraComponent = const_cast<UPandoraComponent*>(PandoraComponent))
	{
		BindPandoraLoadoutChanged(MutablePandoraComponent);
	}

	const UPandoraTreeComponent* PandoraTreeComponent = BoundPandoraTreeComponent.Get();
	const auto RefreshSlot = [PandoraComponent, PandoraTreeComponent](
		UPandoraEquipSlotWidget* PandoraSlot,
		UTextBlock* PandoraLevelText,
		UTextBlock* PandoraLevelLabel,
		const EEnum_Direction Direction)
	{
		UPandoraInstance* PandoraInstance =
			PandoraComponent ? PandoraComponent->GetPandoraLoadoutInstance(Direction) : nullptr;
		const UPandoraDefinition* PandoraDefinition =
			PandoraInstance ? PandoraInstance->PandoraDefinition.Get() : nullptr;
		const int32 PandoraLevel = PandoraTreeComponent && PandoraDefinition
			? FMath::Clamp(
				PandoraTreeComponent->GetCurrentPandoraLevel(
					const_cast<UPandoraDefinition*>(PandoraDefinition)),
				0,
				PandoraDefinition->GetMaxLevel())
			: 0;

		if (PandoraSlot)
		{
			PandoraSlot->SetData(PandoraInstance);
			PandoraSlot->SetPandoraImageDarkened(PandoraInstance && PandoraLevel == 0);
		}

		if (PandoraLevelText)
		{
			PandoraLevelText->SetText(PandoraInstance ? FText::AsNumber(PandoraLevel) : FText::GetEmpty());
		}

		if (PandoraLevelLabel)
		{
			PandoraLevelLabel->SetVisibility(
				PandoraInstance ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
		}
	};

	RefreshSlot(FirstPandora, FirstPandoraLevel, Txt1, EEnum_Direction::Left);
	RefreshSlot(SecondPandora, SecondPandoraLevel, Txt2, EEnum_Direction::Up);
	RefreshSlot(ThirdPandora, ThirdPandoraLevel, Txt3, EEnum_Direction::Right);
}

void ULeftPandoraWidget::SetWeaponImage(const int32 Nth, UTexture2D* WeaponImage)
{
	CacheDefaultWeaponImageBrushes();

	const TArray<UImage*> WeaponImages =
		{ FirstWeaponImage.Get(), SecondWeaponImage.Get(), ThirdWeaponImage.Get() };
	const int32 ImageIndex = Nth - 1;
	if (!WeaponImages.IsValidIndex(ImageIndex) || !WeaponImages[ImageIndex])
	{
		return;
	}

	if (WeaponImage)
	{
		WeaponImages[ImageIndex]->SetBrushFromTexture(WeaponImage, false);
		return;
	}

	if (DefaultWeaponImageBrushes.IsValidIndex(ImageIndex))
	{
		WeaponImages[ImageIndex]->SetBrush(DefaultWeaponImageBrushes[ImageIndex]);
	}
}

void ULeftPandoraWidget::HandlePandoraEquipSlotClicked(UPandoraEquipSlotWidget* PandoraEquipSlot)
{
	SelectPandoraEquipSlot(PandoraEquipSlot);
}

void ULeftPandoraWidget::HandlePandoraLoadoutChanged()
{
	RefreshPandoraLoadoutSlots(BoundPandoraComponent.Get());
}

void ULeftPandoraWidget::HandlePandoraTreeChanged()
{
	RefreshPandoraLoadoutSlots(BoundPandoraComponent.Get());
}

UPandoraComponent* ULeftPandoraWidget::ResolveOwningPandoraComponent() const
{
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		if (APdPlayerState* PlayerState = PlayerController->GetPlayerState<APdPlayerState>())
		{
			return PlayerState->GetPandoraComponent();
		}
	}

	if (APawn* OwningPawn = GetOwningPlayerPawn())
	{
		if (APdPlayerState* PlayerState = OwningPawn->GetPlayerState<APdPlayerState>())
		{
			return PlayerState->GetPandoraComponent();
		}
	}

	return nullptr;
}

UPandoraTreeComponent* ULeftPandoraWidget::ResolvePandoraTreeComponent(
	const UPandoraComponent* PandoraComponent) const
{
	if (const APdPlayerState* PlayerState =
		PandoraComponent ? Cast<APdPlayerState>(PandoraComponent->GetOwner()) : nullptr)
	{
		return PlayerState->GetPandoraTreeComponent();
	}

	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		if (APdPlayerState* PlayerState = PlayerController->GetPlayerState<APdPlayerState>())
		{
			return PlayerState->GetPandoraTreeComponent();
		}
	}

	return nullptr;
}

void ULeftPandoraWidget::BindPandoraLoadoutChanged()
{
	BindPandoraLoadoutChanged(ResolveOwningPandoraComponent());
}

void ULeftPandoraWidget::BindPandoraLoadoutChanged(UPandoraComponent* PandoraComponent)
{
	if (BoundPandoraComponent.Get() == PandoraComponent)
	{
		BindPandoraTreeChanged(ResolvePandoraTreeComponent(PandoraComponent));
		return;
	}

	UnbindPandoraLoadoutChanged();
	BoundPandoraComponent = PandoraComponent;
	if (BoundPandoraComponent)
	{
		BoundPandoraComponent->OnPandoraLoadoutChanged.AddUniqueDynamic(this, &ThisClass::HandlePandoraLoadoutChanged);
	}

	BindPandoraTreeChanged(ResolvePandoraTreeComponent(PandoraComponent));
}

void ULeftPandoraWidget::UnbindPandoraLoadoutChanged()
{
	if (BoundPandoraComponent)
	{
		BoundPandoraComponent->OnPandoraLoadoutChanged.RemoveDynamic(this, &ThisClass::HandlePandoraLoadoutChanged);
		BoundPandoraComponent = nullptr;
	}
}

void ULeftPandoraWidget::BindPandoraTreeChanged(UPandoraTreeComponent* PandoraTreeComponent)
{
	if (BoundPandoraTreeComponent.Get() == PandoraTreeComponent)
	{
		return;
	}

	UnbindPandoraTreeChanged();
	BoundPandoraTreeComponent = PandoraTreeComponent;
	if (BoundPandoraTreeComponent)
	{
		BoundPandoraTreeComponent->OnPandorasChanged.AddUniqueDynamic(this, &ThisClass::HandlePandoraTreeChanged);
	}
}

void ULeftPandoraWidget::UnbindPandoraTreeChanged()
{
	if (BoundPandoraTreeComponent)
	{
		BoundPandoraTreeComponent->OnPandorasChanged.RemoveDynamic(this, &ThisClass::HandlePandoraTreeChanged);
		BoundPandoraTreeComponent = nullptr;
	}
}

void ULeftPandoraWidget::CacheDefaultWeaponImageBrushes()
{
	if (bDefaultWeaponImageBrushesCached)
	{
		return;
	}

	DefaultWeaponImageBrushes.Reset();
	for (const UImage* WeaponImage :
		{ FirstWeaponImage.Get(), SecondWeaponImage.Get(), ThirdWeaponImage.Get() })
	{
		DefaultWeaponImageBrushes.Add(WeaponImage ? WeaponImage->GetBrush() : FSlateBrush());
	}

	bDefaultWeaponImageBrushesCached = true;
}

void ULeftPandoraWidget::RebuildPandoraEquipSlotList()
{
	PandoraEquipSlotList.Reset();
	PandoraEquipSlotList.Reserve(3);
	PandoraEquipSlotList.Add(FirstPandora);
	PandoraEquipSlotList.Add(SecondPandora);
	PandoraEquipSlotList.Add(ThirdPandora);
}

void ULeftPandoraWidget::BindPandoraEquipSlotCallbacks()
{
	RebuildPandoraEquipSlotList();

	for (UPandoraEquipSlotWidget* PandoraEquipSlot : PandoraEquipSlotList)
	{
		if (PandoraEquipSlot)
		{
			PandoraEquipSlot->OnClicked_PandoraEquipSlot.AddUniqueDynamic(this, &ThisClass::HandlePandoraEquipSlotClicked);
		}
	}
}

void ULeftPandoraWidget::UnbindPandoraEquipSlotCallbacks()
{
	for (UPandoraEquipSlotWidget* PandoraEquipSlot : PandoraEquipSlotList)
	{
		if (PandoraEquipSlot)
		{
			PandoraEquipSlot->OnClicked_PandoraEquipSlot.RemoveDynamic(this, &ThisClass::HandlePandoraEquipSlotClicked);
		}
	}
}
