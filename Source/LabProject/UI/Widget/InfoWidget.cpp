#include "UI/Widget/InfoWidget.h"

#include "Common/ProjectTagConfig.h"
#include "Components/Button.h"
#include "Components/WidgetSwitcher.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InfoWidget)

UInfoWidget::UInfoWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UInfoWidget::NativeConstruct()
{
	Super::NativeConstruct();

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

}

void UInfoWidget::NativeDestruct()
{
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
}

void UInfoWidget::OnItemTabButtonClicked()
{
	SelectItemTab();
}

void UInfoWidget::OnSkinButtonClicked()
{
	SelectSkinTab();
}

void UInfoWidget::OnPandoraButtonClicked()
{
	SelectPandoraTab();
}

void UInfoWidget::OnMapButtonClicked()
{
	OnClickedMapButton.Broadcast();
}

void UInfoWidget::SelectInfoCenterPage(UWidget* LeftWidget, UWidget* RightWidget, const FGameplayTag& LeftUiTag, const FGameplayTag& RightUiTag)
{
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
