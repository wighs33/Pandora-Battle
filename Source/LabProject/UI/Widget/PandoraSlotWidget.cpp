#include "UI/Widget/PandoraSlotWidget.h"

#include "Common/LabGameplayTags.h"
#include "Component/Pandora/PandoraComponent.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "Mode/PdHUD.h"
#include "Mode/PdPlayerState.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "UI/Widget/InfoWidget.h"
#include "Settings/MenuLocalizationSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PandoraSlotWidget)

namespace
{
UInfoWidget* ResolveInfoWidgetFromPandoraSlot(const UUserWidget* Widget)
{
	const APlayerController* PlayerController = Widget ? Widget->GetOwningPlayer() : nullptr;
	const APdHUD* Hud = PlayerController ? PlayerController->GetHUD<APdHUD>() : nullptr;
	return Hud ? Hud->GetInfoWidget() : nullptr;
}

bool HasRequiredWeaponTag(const FGameplayTagContainer& RequiredWeaponTags, const FGameplayTag& WeaponTag)
{
	if (RequiredWeaponTags.IsEmpty())
	{
		return false;
	}

	if (!WeaponTag.IsValid())
	{
		return false;
	}

	for (const FGameplayTag& RequiredWeaponTag : RequiredWeaponTags)
	{
		if (RequiredWeaponTag == WeaponTag || RequiredWeaponTag.MatchesTag(WeaponTag))
		{
			return true;
		}
	}

	return false;
}
}

void UPandoraSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	bIsHoverActive = false;
	const APdPlayerState* PlayerState = GetOwningPlayerState<APdPlayerState>();
	UnbindPandoraEvents();
	BoundPandoraComponent = PlayerState ? PlayerState->GetPandoraComponent() : nullptr;
	if (UPandoraComponent* Component = BoundPandoraComponent.Get())
	{
		Component->OnPandoraLoadoutChanged.AddUniqueDynamic(this, &ThisClass::RefreshOwnership);
		Component->OnPandoraInventoryChanged.AddUniqueDynamic(this, &ThisClass::RefreshOwnership);
	}
	if (HoverBorder) HoverBorder->SetVisibility(ESlateVisibility::Collapsed);
	ApplyViewData(FPandoraSlotViewDataBuilder::Build(CachedData, PlayerState ? PlayerState->GetPandoraComponent() : nullptr));
}

void UPandoraSlotWidget::NativeDestruct()
{
	UnbindPandoraEvents();
	if (bIsHoverActive)
	{
		if (UInfoWidget* InfoWidget = ResolveInfoWidgetFromPandoraSlot(this))
		{
			InfoWidget->HidePandoraDescriptionDetailAtWidget(this);
		}
		bIsHoverActive = false;
	}

	Super::NativeDestruct();
}

void UPandoraSlotWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);

	SetData(Cast<UPandoraDefinition>(ListItemObject));
}

void UPandoraSlotWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);

	if (bIsHoverActive)
	{
		return;
	}

	const APlayerController* OwningPlayer = GetOwningPlayer();
	if (!OwningPlayer || !OwningPlayer->IsLocalController())
	{
		return;
	}

	bIsHoverActive = true;
	if (HoverBorder) HoverBorder->SetVisibility(ESlateVisibility::HitTestInvisible);
	UInfoWidget* InfoWidget = ResolveInfoWidgetFromPandoraSlot(this);
	if (InfoWidget)
	{
		if (CachedData)
		{
			InfoWidget->ShowPandoraDescriptionDetailImmediatelyAtWidget(CachedData, this, true);
		}
		else
		{
			InfoWidget->HideDetailWidgets();
		}
	}
}

void UPandoraSlotWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	if (!bIsHoverActive)
	{
		Super::NativeOnMouseLeave(InMouseEvent);
		return;
	}

	bIsHoverActive = false;
	UInfoWidget* InfoWidget = ResolveInfoWidgetFromPandoraSlot(this);
	if (HoverBorder) HoverBorder->SetVisibility(ESlateVisibility::Collapsed);
	if (InfoWidget)
	{
		InfoWidget->HidePandoraDescriptionDetailAtWidget(this);
	}

	Super::NativeOnMouseLeave(InMouseEvent);
}

void UPandoraSlotWidget::SetData(const UPandoraDefinition* Target)
{
	CachedData = Target;

	const APdPlayerState* PlayerState = GetOwningPlayerState<APdPlayerState>();
	ApplyViewData(FPandoraSlotViewDataBuilder::Build(CachedData, PlayerState ? PlayerState->GetPandoraComponent() : nullptr));
}

void UPandoraSlotWidget::ApplyViewData(const FPandoraSlotViewData& ViewData)
{
	if (TextBlock)
	{
		TextBlock->SetText(GetLocalization()
			? GetLocalization()->GetProductText(CachedData, TEXT("Name"), ViewData.DisplayName)
			: ViewData.DisplayName);
	}

	if (IconImage)
	{
		IconImage->SetBrushResourceObject(ViewData.IconResource);
		IconImage->SetRenderOpacity(ViewData.bOwned ? 1.0f : 0.38f);
		IconImage->SetVisibility(ViewData.IconResource ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	RefreshWeaponRequirementImages(ViewData.RequiredWeaponTags);
	SetIsEnabled(true);
	SetRenderOpacity(1.0f);
	if (OwnershipText)
	{
		const FName Key = ViewData.bEquipped ? TEXT("Info.Assigned") : ViewData.bOwned ? TEXT("Pandora.Owned") : TEXT("Pandora.Unowned");
		const FText Fallback = FText::FromString(ViewData.bEquipped ? TEXT("Equipped") : ViewData.bOwned ? TEXT("Owned") : TEXT("Not owned"));
		OwnershipText->SetText(GetLocalization() ? GetLocalization()->GetTextOrFallback(Key, Fallback) : Fallback);
		OwnershipText->SetColorAndOpacity(FSlateColor(ViewData.bEquipped
			? FLinearColor(0.76f, 0.56f, 1.0f) : ViewData.bOwned
			? FLinearColor(0.77f, 0.66f, 0.43f) : FLinearColor(0.48f, 0.50f, 0.58f)));
	}
	if (EquippedMark) EquippedMark->SetVisibility(ViewData.bEquipped ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UPandoraSlotWidget::RefreshOwnership()
{
	SetData(CachedData);
}

void UPandoraSlotWidget::UnbindPandoraEvents()
{
	if (UPandoraComponent* Component = BoundPandoraComponent.Get())
	{
		Component->OnPandoraLoadoutChanged.RemoveDynamic(this, &ThisClass::RefreshOwnership);
		Component->OnPandoraInventoryChanged.RemoveDynamic(this, &ThisClass::RefreshOwnership);
	}
	BoundPandoraComponent.Reset();
}

void UPandoraSlotWidget::RefreshWeaponRequirementImages(const FGameplayTagContainer& RequiredWeaponTags) const
{
	HideAllWeaponRequirementImages();

	SetWeaponRequirementImageVisible(Axe, HasRequiredWeaponTag(RequiredWeaponTags, LabGameplayTags::Item_Weapon_Axe));
	SetWeaponRequirementImageVisible(Bow, HasRequiredWeaponTag(RequiredWeaponTags, LabGameplayTags::Item_Weapon_Bow));
	SetWeaponRequirementImageVisible(Dagger, HasRequiredWeaponTag(RequiredWeaponTags, LabGameplayTags::Item_Weapon_Dagger));
	SetWeaponRequirementImageVisible(Greatsword, HasRequiredWeaponTag(RequiredWeaponTags, LabGameplayTags::Item_Weapon_GreatSword));
	SetWeaponRequirementImageVisible(Sword, HasRequiredWeaponTag(RequiredWeaponTags, LabGameplayTags::Item_Weapon_Sword));
	SetWeaponRequirementImageVisible(Gun, HasRequiredWeaponTag(RequiredWeaponTags, LabGameplayTags::Item_Weapon_Gun));
}

void UPandoraSlotWidget::HideAllWeaponRequirementImages() const
{
	SetWeaponRequirementImageVisible(Axe, false);
	SetWeaponRequirementImageVisible(Bow, false);
	SetWeaponRequirementImageVisible(Dagger, false);
	SetWeaponRequirementImageVisible(Greatsword, false);
	SetWeaponRequirementImageVisible(Sword, false);
	SetWeaponRequirementImageVisible(Gun, false);
}

void UPandoraSlotWidget::SetWeaponRequirementImageVisible(UImage* Image, const bool bVisible) const
{
	if (!Image)
	{
		return;
	}

	Image->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}

void UPandoraSlotWidget::OnMenuLanguageChanged()
{
 SetData(CachedData);
}
