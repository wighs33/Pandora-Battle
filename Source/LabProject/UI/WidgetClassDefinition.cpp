#include "UI/WidgetClassDefinition.h"

#include "Common/LabGameplayTags.h"
#include "UI/InfoUiPresenter.h"
#include "UI/Widget/InfoWidget.h"
#include "UI/Widget/SelectPandoraWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WidgetClassDefinition)

UWidgetClassDefinition::UWidgetClassDefinition()
{
	InfoWidgetSettings.WidgetTag = LabGameplayTags::UI_Widget_Info;
	InfoWidgetSettings.PresenterClass = UInfoUiPresenter::StaticClass();
	SelectPandoraWidgetSettings.WidgetTag = LabGameplayTags::UI_Widget_SelectPandora;
	AimCrosshairWidgetSettings.WidgetTag = LabGameplayTags::UI_Widget_AimCrosshair;
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

	return nullptr;
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
