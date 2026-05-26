#include "UI/Widget/PandoraWidgetViewData.h"

#include "AbilitySystem/PandoraTree/PandoraTreeComponent.h"
#include "Pandora/PandoraDefinition.h"

namespace
{
	const FText MaxLevelText = NSLOCTEXT("PandoraWidget", "MaxLevel", "MAX");
}

FPandoraWidgetViewData FPandoraWidgetViewDataBuilder::Build(
	UPandoraDefinition* PandoraDefinition,
	const UPandoraTreeComponent* PandoraTreeComponent,
	const FPandoraWidgetStyleConfig& Style)
{
	FPandoraWidgetViewData ViewData;

	if (PandoraDefinition)
	{
		ViewData.DisplayName = PandoraDefinition->GetDisplayName();
		ViewData.MaxLevel = PandoraDefinition->GetMaxLevel();
	}

	ViewData.CurrentLevel = PandoraTreeComponent && PandoraDefinition
		? PandoraTreeComponent->GetCurrentPandoraLevel(PandoraDefinition)
		: 0;
	const bool bHasPandora = ViewData.CurrentLevel > 0;

	ViewData.bCanSpend = PandoraTreeComponent
		&& PandoraDefinition
		&& PandoraTreeComponent->CanSpendPointOnPandora(PandoraDefinition);
	ViewData.PointsAvailable = PandoraTreeComponent ? PandoraTreeComponent->GetPointsAvailable() : INDEX_NONE;
	ViewData.RequiredPoints = PandoraTreeComponent && PandoraDefinition
		? PandoraTreeComponent->GetRequiredPointsForPandora(PandoraDefinition, ViewData.CurrentLevel > 0)
		: INDEX_NONE;
	ViewData.bUnlockRulesMet = !PandoraTreeComponent
		|| !PandoraDefinition
		|| ViewData.CurrentLevel > 0
		|| PandoraTreeComponent->ArePandoraUnlockRulesMet(PandoraDefinition);

	ViewData.bAtMaxLevel = PandoraDefinition && ViewData.MaxLevel > 0 && ViewData.CurrentLevel >= ViewData.MaxLevel;
	ViewData.bLocked = PandoraTreeComponent && PandoraDefinition && ViewData.CurrentLevel <= 0 && !ViewData.bUnlockRulesMet;
	ViewData.bNotEnoughPoints = PandoraTreeComponent
		&& !ViewData.bAtMaxLevel
		&& !ViewData.bLocked
		&& ViewData.RequiredPoints > 0
		&& ViewData.PointsAvailable >= 0
		&& ViewData.PointsAvailable < ViewData.RequiredPoints;
	const bool bAvailableStyle = !PandoraTreeComponent || ViewData.bCanSpend || ViewData.bAtMaxLevel;
	ViewData.bInactiveStyle = PandoraTreeComponent && PandoraDefinition && ViewData.CurrentLevel <= 0;
	ViewData.bDimmedStyle = ViewData.bInactiveStyle || !bAvailableStyle || ViewData.bLocked || ViewData.bNotEnoughPoints;
	ViewData.OverlayColor = ViewData.bLocked
		? Style.LockedOverlayColor
		: (ViewData.bNotEnoughPoints
			? Style.NotEnoughPointsOverlayColor
			: (ViewData.bDimmedStyle ? Style.UnavailableOverlayColor : Style.AvailableOverlayColor));
	ViewData.ContentOpacity = ViewData.bDimmedStyle ? Style.UnavailableContentOpacity : Style.AvailableContentOpacity;
	ViewData.StateIconVisibility = ViewData.bLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	ViewData.StateIconColor = FLinearColor::White;
	ViewData.LevelText = MakeLevelText(ViewData.CurrentLevel, ViewData.MaxLevel, Style.bShowMaxText);
	ViewData.IconResource = PandoraDefinition
		? (bHasPandora ? PandoraDefinition->GetActiveIconResource() : PandoraDefinition->GetIconResource())
		: nullptr;

	return ViewData;
}

FText FPandoraWidgetViewDataBuilder::MakeLevelText(
	const int32 CurrentLevel,
	const int32 MaxLevel,
	const bool bShowMaxText)
{
	if (bShowMaxText && MaxLevel > 0 && CurrentLevel >= MaxLevel)
	{
		return MaxLevelText;
	}

	return FText::Format(
		NSLOCTEXT("PandoraWidget", "PandoraLevelFormat", "{0}/{1}"),
		FText::AsNumber(CurrentLevel),
		FText::AsNumber(MaxLevel));
}
