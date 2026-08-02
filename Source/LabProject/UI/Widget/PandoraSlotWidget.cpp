#include "UI/Widget/PandoraSlotWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "Mode/PdHUD.h"
#include "Pandora/PandoraInstance.h"
#include "UI/Widget/InfoWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PandoraSlotWidget)

namespace
{
UInfoWidget* ResolveInfoWidgetFromPandoraSlot(const UUserWidget* Widget)
{
	const APlayerController* PlayerController = Widget ? Widget->GetOwningPlayer() : nullptr;
	const APdHUD* Hud = PlayerController ? PlayerController->GetHUD<APdHUD>() : nullptr;
	return Hud ? Hud->GetInfoWidget() : nullptr;
}

bool HasRequiredWeaponTag(const FGameplayTagContainer& RequiredWeaponTags, const TCHAR* WeaponTagName)
{
	if (RequiredWeaponTags.IsEmpty())
	{
		return false;
	}

	const FGameplayTag WeaponTag = FGameplayTag::RequestGameplayTag(FName(WeaponTagName), false);
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
	ApplyViewData(FPandoraSlotViewDataBuilder::Build(CachedData));
}

void UPandoraSlotWidget::NativeDestruct()
{
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

	if (UPandoraInstance* PandoraInstance = Cast<UPandoraInstance>(ListItemObject))
	{
		SetData(PandoraInstance);
	}
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
	if (InfoWidget)
	{
		InfoWidget->HidePandoraDescriptionDetailAtWidget(this);
	}

	Super::NativeOnMouseLeave(InMouseEvent);
}

void UPandoraSlotWidget::SetData(UPandoraInstance* Target)
{
	CachedData = Target;

	ApplyViewData(FPandoraSlotViewDataBuilder::Build(CachedData));
}

void UPandoraSlotWidget::ApplyViewData(const FPandoraSlotViewData& ViewData)
{
	if (TextBlock)
	{
		TextBlock->SetText(ViewData.DisplayName);
	}

	if (IconImage)
	{
		IconImage->SetBrushResourceObject(ViewData.IconResource);
		IconImage->SetVisibility(ViewData.IconResource ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	RefreshWeaponRequirementImages(ViewData.RequiredWeaponTags);
	SetIsEnabled(true);
	SetRenderOpacity(ViewData.bEnabled ? 1.0f : 0.45f);
}

void UPandoraSlotWidget::RefreshWeaponRequirementImages(const FGameplayTagContainer& RequiredWeaponTags) const
{
	HideAllWeaponRequirementImages();

	SetWeaponRequirementImageVisible(Axe, HasRequiredWeaponTag(RequiredWeaponTags, TEXT("Item.Weapon.Axe")));
	SetWeaponRequirementImageVisible(Bow, HasRequiredWeaponTag(RequiredWeaponTags, TEXT("Item.Weapon.Bow")));
	SetWeaponRequirementImageVisible(Dagger, HasRequiredWeaponTag(RequiredWeaponTags, TEXT("Item.Weapon.Dagger")));
	SetWeaponRequirementImageVisible(Greatsword, HasRequiredWeaponTag(RequiredWeaponTags, TEXT("Item.Weapon.GreatSword")));
	SetWeaponRequirementImageVisible(Sword, HasRequiredWeaponTag(RequiredWeaponTags, TEXT("Item.Weapon.Sword")));
	SetWeaponRequirementImageVisible(Gun, HasRequiredWeaponTag(RequiredWeaponTags, TEXT("Item.Weapon.Gun")));
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
