#include "UI/Widget/PandoraWidgetViewData.h"

#include "Component/AbilitySystem/PandoraTreeComponent.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "Pandora/PandoraInstance.h"

namespace
{
	const FText MaxLevelText = NSLOCTEXT("PandoraWidget", "MaxLevel", "MAX");

	FText FormatPointsRequiredText(int32 Points)
	{
		return FText::Format(
			NSLOCTEXT("PandoraDescriptionWidget", "PointsRequiredFormat", "Points Required: {0}"),
			FText::AsNumber(Points));
	}

	FString MakeWeaponTagDisplayName(const FGameplayTag& WeaponTag)
	{
		FString TagText = WeaponTag.ToString();
		TagText.RemoveFromStart(TEXT("Item.Weapon."));
		return TagText.Replace(TEXT("."), TEXT(" / "));
	}

	FText MakeWeaponRequirementText(const FGameplayTagContainer& RequiredWeaponTags)
	{
		if (RequiredWeaponTags.IsEmpty())
		{
			return FText::GetEmpty();
		}

		TArray<FString> WeaponTypeNames;
		for (const FGameplayTag& WeaponTag : RequiredWeaponTags)
		{
			if (WeaponTag.IsValid())
			{
				WeaponTypeNames.Add(MakeWeaponTagDisplayName(WeaponTag));
			}
		}

		if (WeaponTypeNames.IsEmpty())
		{
			return FText::GetEmpty();
		}

		return FText::Format(
			NSLOCTEXT("PandoraDescriptionWidget", "WeaponRequirementFormat", "Required Weapon Type: {0}"),
			FText::FromString(FString::Join(WeaponTypeNames, TEXT(", "))));
	}
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
		ViewData.Description = PandoraDefinition->GetDescription();
		ViewData.MaxLevel = PandoraDefinition->GetMaxLevel();
		ViewData.RequiredWeaponTags = PandoraDefinition->ActivatableWeaponTags;
	}

	ViewData.CurrentLevel = PandoraTreeComponent && PandoraDefinition
		? PandoraTreeComponent->GetCurrentPandoraLevel(PandoraDefinition)
		: 0;
	const bool bHasPandora = ViewData.CurrentLevel > 0;
	const bool bUnlockedForTree = PandoraTreeComponent
		&& PandoraDefinition
		&& PandoraTreeComponent->IsPandoraUnlockedForTree(PandoraDefinition);
	ViewData.bOwned = bHasPandora || bUnlockedForTree;

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
		|| bUnlockedForTree
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
	ViewData.bActive = bAvailableStyle && !ViewData.bLocked && !ViewData.bNotEnoughPoints;
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
	ViewData.IconResource = PandoraDefinition ? PandoraDefinition->GetIconResource() : nullptr;

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

FPandoraSlotViewData FPandoraSlotViewDataBuilder::Build(const UPandoraInstance* PandoraInstance)
{
	FPandoraSlotViewData ViewData;
	const UPandoraDefinition* PandoraDefinition = IsValid(PandoraInstance)
		? PandoraInstance->PandoraDefinition.Get()
		: nullptr;

	ViewData.bOwned = IsValid(PandoraInstance) && PandoraInstance->IsOwned;
	ViewData.bActive = ViewData.bOwned;
	ViewData.bEnabled = ViewData.bOwned;

	if (!PandoraDefinition)
	{
		return ViewData;
	}

	ViewData.DisplayName = PandoraDefinition->GetDisplayName();
	ViewData.Description = PandoraDefinition->GetDescription();
	ViewData.IconResource = PandoraDefinition->GetIconResource();
	ViewData.RequiredWeaponTags = PandoraDefinition->ActivatableWeaponTags;
	return ViewData;
}

FPandoraDescriptionViewData FPandoraDescriptionViewDataBuilder::Build(
	UPandoraDefinition* PandoraDefinition,
	const UPandoraTreeComponent* PandoraTreeComponent)
{
	FPandoraDescriptionViewData ViewData;
	ViewData.SkillSlots.SetNum(UPandoraDefinition::GetFixedMaxLevel());
	ViewData.bHasPandoraDefinition = PandoraDefinition != nullptr;

	if (!PandoraDefinition)
	{
		return ViewData;
	}

	ViewData.TitleText = PandoraDefinition->GetDisplayName();
	ViewData.DescriptionText = PandoraDefinition->GetDescription();
	ViewData.SkillSectionVisibility = ESlateVisibility::Visible;
	ViewData.MaxLevel = FMath::Max(PandoraDefinition->GetMaxLevel(), 1);

	if (PandoraTreeComponent)
	{
		ViewData.CurrentLevel = PandoraTreeComponent->GetCurrentPandoraLevel(PandoraDefinition);
		ViewData.MaxLevel = FMath::Max(PandoraTreeComponent->GetMaxPandoraLevel(PandoraDefinition), 1);
		ViewData.PointsRequiredText = FormatPointsRequiredText(PandoraTreeComponent->GetRequiredPointsForPandora(PandoraDefinition, true));
	}

	ViewData.NextLevel = ViewData.CurrentLevel + 1 > ViewData.MaxLevel ? -1 : ViewData.CurrentLevel + 1;

	for (int32 SkillIndex = 0; SkillIndex < ViewData.SkillSlots.Num(); ++SkillIndex)
	{
		if (!PandoraDefinition->Skill.IsValidIndex(SkillIndex))
		{
			continue;
		}

		const FSkill& Skill = PandoraDefinition->Skill[SkillIndex];
		FPandoraSkillSlotViewData& SkillViewData = ViewData.SkillSlots[SkillIndex];
		SkillViewData.IconResource = Skill.GetIconResource();
		SkillViewData.DisplayName = Skill.GetDisplayName();
		SkillViewData.Description = Skill.GetDescription();
	}

	ViewData.bLockedByPandoraRequirement = PandoraTreeComponent
		&& ViewData.CurrentLevel <= 0
		&& !PandoraTreeComponent->ArePandoraUnlockRulesMet(PandoraDefinition);

	const FText WeaponRequirement = MakeWeaponRequirementText(PandoraDefinition->ActivatableWeaponTags);
	ViewData.WeaponRequirementText = WeaponRequirement;
	ViewData.WeaponRequirementVisibility = WeaponRequirement.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible;

	if (ViewData.bLockedByPandoraRequirement)
	{
		FText RequirementText = PandoraTreeComponent->GetPandoraUnlockRequirementsText(PandoraDefinition);
		if (RequirementText.IsEmpty())
		{
			RequirementText = NSLOCTEXT("PandoraDescriptionWidget", "LockedRequirementFallback", "Unlock requirements are not met.");
		}

		ViewData.DescriptionText = RequirementText;
		return ViewData;
	}

	if (ViewData.NextLevel > 0 && ViewData.CurrentLevel < ViewData.MaxLevel)
	{
		ViewData.PointsRequiredVisibility = ESlateVisibility::Visible;
	}
	else
	{
		ViewData.PointsRequiredVisibility = ESlateVisibility::Collapsed;
	}

	return ViewData;
}
