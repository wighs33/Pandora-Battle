#include "UI/WidgetClassDefinition.h"

#include "Common/LabGameplayTags.h"
#include "UI/InfoUiPresenter.h"
#include "UI/Widget/InfoWidget.h"
#include "UI/Widget/SelectPandoraWidget.h"
#include "UI/Widget/PandoraTreeWidget.h"
#include "UI/Widget/RightNotificationsWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WidgetClassDefinition)

UWidgetClassDefinition::UWidgetClassDefinition()
{
	PlayerHudWidgetSettings.WidgetTag = LabGameplayTags::UI_Widget_PlayerHUD;
	InfoWidgetSettings.WidgetTag = LabGameplayTags::UI_Widget_Info;
	InfoWidgetSettings.PresenterClass = UInfoUiPresenter::StaticClass();
	SelectPandoraWidgetSettings.WidgetTag = LabGameplayTags::UI_Widget_SelectPandora;
	AimCrosshairWidgetSettings.WidgetTag = LabGameplayTags::UI_Widget_AimCrosshair;
	PandoraTreeWidgetSettings.WidgetTag = LabGameplayTags::UI_Widget_PandoraTree;
}

FPrimaryAssetId UWidgetClassDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("WidgetClassDefinition"), GetFName());
}

TSubclassOf<UUserWidget> UWidgetClassDefinition::FindWidgetClassByTag(FGameplayTag WidgetTag) const
{
	if (!WidgetTag.IsValid())
	{
		return nullptr;
	}

	if (PlayerHudWidgetSettings.WidgetTag.MatchesTagExact(WidgetTag))
	{
		return PlayerHudWidgetSettings.WidgetClass;
	}

	if (InfoWidgetSettings.WidgetTag.MatchesTagExact(WidgetTag))
	{
		return TSubclassOf<UUserWidget>(InfoWidgetSettings.WidgetClass.Get());
	}

	if (SelectPandoraWidgetSettings.WidgetTag.MatchesTagExact(WidgetTag))
	{
		return TSubclassOf<UUserWidget>(SelectPandoraWidgetSettings.WidgetClass.Get());
	}

	if (AimCrosshairWidgetSettings.WidgetTag.MatchesTagExact(WidgetTag))
	{
		return AimCrosshairWidgetSettings.WidgetClass;
	}

	if (PandoraTreeWidgetSettings.WidgetTag.MatchesTagExact(WidgetTag))
	{
		return TSubclassOf<UUserWidget>(PandoraTreeWidgetSettings.WidgetClass.Get());
	}

	return nullptr;
}

TSubclassOf<UUserWidget> UWidgetClassDefinition::GetPlayerHudWidgetClass() const
{
	return PlayerHudWidgetSettings.WidgetClass;
}

TSubclassOf<UInfoWidget> UWidgetClassDefinition::GetInfoWidgetClass() const
{
	return InfoWidgetSettings.WidgetClass;
}

TSubclassOf<USelectPandoraWidget> UWidgetClassDefinition::GetSelectPandoraWidgetClass() const
{
	return SelectPandoraWidgetSettings.WidgetClass;
}

TSubclassOf<UUserWidget> UWidgetClassDefinition::GetAimCrosshairWidgetClass() const
{
	return AimCrosshairWidgetSettings.WidgetClass;
}

TSubclassOf<UPandoraTreeWidget> UWidgetClassDefinition::GetPandoraTreeWidgetClass() const
{
	return PandoraTreeWidgetSettings.WidgetClass;
}

TSubclassOf<URightNotificationsWidget> UWidgetClassDefinition::GetRightNotificationsWidgetClass() const
{
	return RightNotificationsWidgetSettings.WidgetClass;
}
