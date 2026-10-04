#include "UI/Pandora/PandoraWidgetViewData.h"

#include "Blueprint/UserWidget.h"
#include "Common/LabGameplayTags.h"
#include "Common/Enum_Direction.h"
#include "Component/Pandora/PandoraComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"
#include "Profile/PlayerProfileSubsystem.h"
#include "Mode/PdPlayerState.h"
#include "Component/AbilitySystem/PandoraTreeComponent.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "Localization/MenuLocalizationSubsystem.h"

namespace
{
	const FText MaxLevelText = NSLOCTEXT("PandoraWidget", "MaxLevel", "MAX");

	FString MakeWeaponTagDisplayName(const FGameplayTag& WeaponTag)
	{
		FString TagText = WeaponTag.ToString();
		const FGameplayTag& WeaponTypeRoot = LabGameplayTags::Item_Weapon;
		if (WeaponTag.MatchesTag(WeaponTypeRoot))
		{
			const FString WeaponTypePrefix = WeaponTypeRoot.ToString() + TEXT(".");
			TagText.RemoveFromStart(WeaponTypePrefix);
		}
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

	// 해금 판정은 게임플레이 컴포넌트가 맡고, 조건의 표시 문구만 UI에서 만든다.
	FText MakePandoraUnlockRequirementsText(const UPandoraDefinition* Definition, const UPandoraTreeComponent* Tree, const UMenuLocalizationSubsystem* Localization)
	{
		TArray<FText> Lines;
		for (const FPandoraUnlockRule& Rule : Definition->GetUnlockRules())
		{
			if (Rule.RequiredPandora)
			{
				Lines.Add(FText::Format(
					NSLOCTEXT("PandoraTreeComponent", "PandoraUnlockRequirementLine", "- {0} Lv. {1} ({2}/{1})"),
					Localization ? Localization->GetProductText(Rule.RequiredPandora, TEXT("Name"), Rule.RequiredPandora->GetDisplayName()) : Rule.RequiredPandora->GetDisplayName(),
					FText::AsNumber(FMath::Max(Rule.RequiredLevel, 1)),
					FText::AsNumber(Tree->GetCurrentPandoraLevel(Rule.RequiredPandora))));
			}
		}
		return Lines.IsEmpty() ? FText::GetEmpty() : FText::Format(
			NSLOCTEXT("PandoraTreeComponent", "PandoraUnlockRequirements", "요구 조건\n{0}"),
			FText::Join(FText::FromString(TEXT("\n")), Lines));
	}
}

FPandoraWidgetViewData FPandoraWidgetViewDataBuilder::Build(const UPandoraDefinition* PandoraDefinition,
	const UPandoraTreeComponent* PandoraTreeComponent, const FPandoraWidgetStyleConfig& Style)
{
	FPandoraWidgetViewData ViewData;

	if (PandoraDefinition)
	{
		ViewData.DisplayName = PandoraDefinition->GetDisplayName();
		ViewData.Description = PandoraDefinition->GetDescription();
		ViewData.MaxLevel = PandoraDefinition->GetMaxLevel();
		ViewData.RequiredWeaponTags = PandoraDefinition->GetActivatableWeaponTags();
	}

	ViewData.CurrentLevel = PandoraTreeComponent && PandoraDefinition
		? PandoraTreeComponent->GetCurrentPandoraLevel(PandoraDefinition)
		: 0;
	const bool bHasPandora = ViewData.CurrentLevel > 0;
	const bool bUnlockedForTree = PandoraTreeComponent && PandoraDefinition
		&& PandoraTreeComponent->IsPandoraUnlockedForTree(PandoraDefinition);
	ViewData.bOwned = bHasPandora || bUnlockedForTree;

	ViewData.bCanSpend = PandoraTreeComponent && PandoraDefinition
		&& PandoraTreeComponent->CanSpendPointOnPandora(PandoraDefinition);
	ViewData.PointsAvailable = PandoraTreeComponent ? PandoraTreeComponent->GetPointsAvailable() : INDEX_NONE;
	ViewData.RequiredPoints = PandoraTreeComponent && PandoraDefinition
		? PandoraTreeComponent->GetRequiredPointsForPandora(PandoraDefinition, ViewData.CurrentLevel > 0)
		: INDEX_NONE;
	ViewData.bUnlockRulesMet = !PandoraTreeComponent || !PandoraDefinition
		|| PandoraTreeComponent->IsPandoraAvailableForInvestment(PandoraDefinition);

	ViewData.bAtMaxLevel = PandoraDefinition && ViewData.MaxLevel > 0 && ViewData.CurrentLevel >= ViewData.MaxLevel;
	ViewData.bLocked = PandoraTreeComponent && PandoraDefinition && ViewData.CurrentLevel <= 0 && !ViewData.bUnlockRulesMet;
	ViewData.bNotEnoughPoints = PandoraTreeComponent && !ViewData.bAtMaxLevel && !ViewData.bLocked
		&& ViewData.RequiredPoints > 0 && ViewData.PointsAvailable >= 0
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

APdPlayerState* FPandoraWidgetViewDataBuilder::FindOwningPlayerState(const UUserWidget* Widget)
{
	if (!Widget)
	{
		return nullptr;
	}

	if (const APlayerController* PlayerController = Widget->GetOwningPlayer())
	{
		if (APdPlayerState* PlayerState = PlayerController->GetPlayerState<APdPlayerState>())
		{
			return PlayerState;
		}
	}

	const APawn* OwningPawn = Widget->GetOwningPlayerPawn();
	return OwningPawn ? OwningPawn->GetPlayerState<APdPlayerState>() : nullptr;
}

const UPandoraDefinition* FPandoraWidgetViewDataBuilder::GetSelectedPandoraDefinition(const UPandoraTreeComponent* PandoraTreeComponent)
{
	const APdPlayerState* PlayerState = PandoraTreeComponent ? PandoraTreeComponent->GetPlayerState<APdPlayerState>() : nullptr;
	const UPandoraComponent* PandoraComponent = PlayerState ? PlayerState->GetPandoraComponent() : nullptr;
	return PandoraComponent ? PandoraComponent->GetCurrentPandoraDefinition() : nullptr;
}

// 경기 트리가 없는 미리보기에서만 로컬 프로필의 소유 목록을 사용한다.
bool FPandoraWidgetViewDataBuilder::IsPandoraOwnedInProfile(const UUserWidget* Widget, const UPandoraDefinition* PandoraDefinition)
{
	UPlayerProfileSubsystem* ProfileSubsystem =
		Widget ? UGameInstance::GetSubsystem<UPlayerProfileSubsystem>(Widget->GetGameInstance()) : nullptr;
	if (!ProfileSubsystem || !IsValid(PandoraDefinition))
	{
		return false;
	}

	return ProfileSubsystem->IsPandoraGranted(PandoraDefinition);
}

FText FPandoraWidgetViewDataBuilder::MakeLevelText(const int32 CurrentLevel, const int32 MaxLevel,
	const bool bShowMaxText)
{
	if (bShowMaxText && MaxLevel > 0 && CurrentLevel >= MaxLevel)
	{
		return MaxLevelText;
	}

	return FText::Format(NSLOCTEXT("PandoraWidget", "PandoraLevelFormat", "{0}/{1}"), FText::AsNumber(CurrentLevel),
		FText::AsNumber(MaxLevel));
}

FPandoraSlotViewData FPandoraSlotViewDataBuilder::Build(const UPandoraDefinition* PandoraDefinition,
	const UPandoraComponent* PandoraComponent)
{
	FPandoraSlotViewData ViewData;
	ViewData.bOwned = PandoraComponent && PandoraComponent->HasPandoraDefinition(PandoraDefinition);
	ViewData.bActive = ViewData.bOwned;
	ViewData.bEnabled = ViewData.bOwned;

	if (!PandoraDefinition)
	{
		return ViewData;
	}

	ViewData.DisplayName = PandoraDefinition->GetDisplayName();
	ViewData.Description = PandoraDefinition->GetDescription();
	ViewData.IconResource = PandoraDefinition->GetIconResource();
	ViewData.RequiredWeaponTags = PandoraDefinition->GetActivatableWeaponTags();
	if (PandoraComponent)
	{
		for (const EEnum_Direction Direction : { EEnum_Direction::Left, EEnum_Direction::Up, EEnum_Direction::Right })
		{
			ViewData.bEquipped |= PandoraComponent->GetPandoraLoadoutDefinition(Direction) == PandoraDefinition;
		}
	}
	return ViewData;
}

FPandoraDescriptionViewData FPandoraDescriptionViewDataBuilder::Build(const UPandoraDefinition* PandoraDefinition,
	const UPandoraTreeComponent* PandoraTreeComponent, const UMenuLocalizationSubsystem* Localization)
{
	FPandoraDescriptionViewData ViewData;
	ViewData.SkillSlots.SetNum(UPandoraDefinition::GetFixedMaxLevel());

	if (!PandoraDefinition)
	{
		return ViewData;
	}

	ViewData.TitleText = Localization ? Localization->GetProductText(PandoraDefinition, TEXT("Name"), PandoraDefinition->GetDisplayName()) : PandoraDefinition->GetDisplayName();
	ViewData.DescriptionText = Localization ? Localization->GetProductText(PandoraDefinition, TEXT("Description"), PandoraDefinition->GetDescription()) : PandoraDefinition->GetDescription();
	ViewData.SkillSectionVisibility = ESlateVisibility::Visible;

	FNumberFormattingOptions NumberFormat;
	NumberFormat.SetMinimumFractionalDigits(0).SetMaximumFractionalDigits(1);
	const FText ManaFormat = Localization
		? Localization->GetTextOrFallback(TEXT("PandoraDescription.ManaFormat"), FText::GetEmpty())
		: FText::GetEmpty();
	const FText CooldownFormat = Localization
		? Localization->GetTextOrFallback(TEXT("PandoraDescription.CooldownFormat"), FText::GetEmpty())
		: FText::GetEmpty();

	for (int32 SkillIndex = 0; SkillIndex < ViewData.SkillSlots.Num(); ++SkillIndex)
	{
		const USkillDefinition* Skill = PandoraDefinition->GetSkillDefinition(SkillIndex);
		if (!Skill)
		{
			continue;
		}

		FPandoraSkillSlotViewData& SkillViewData = ViewData.SkillSlots[SkillIndex];
		SkillViewData.IconResource = Skill->GetIconResource();
		SkillViewData.DisplayName = Localization ? Localization->GetProductText(Skill, TEXT("Name"), Skill->GetDisplayName()) : Skill->GetDisplayName();
		SkillViewData.Description = Localization ? Localization->GetProductText(Skill, TEXT("Description"), Skill->Description) : Skill->Description;
		SkillViewData.ManaText = FText::Format(ManaFormat, FText::AsNumber(Skill->ManaCost, &NumberFormat));
		SkillViewData.CooldownText = FText::Format(CooldownFormat, FText::AsNumber(Skill->Time.CooldownDuration, &NumberFormat));
	}

	const FText WeaponRequirement = MakeWeaponRequirementText(PandoraDefinition->GetActivatableWeaponTags());
	ViewData.WeaponRequirementText = WeaponRequirement;
	ViewData.WeaponRequirementVisibility = WeaponRequirement.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible;

	// 아직 투자할 수 없는 판도라는 설명 대신 해금 조건을 보여 준다.
	if (PandoraTreeComponent && !PandoraTreeComponent->IsPandoraAvailableForInvestment(PandoraDefinition))
	{
		FText RequirementText = MakePandoraUnlockRequirementsText(PandoraDefinition, PandoraTreeComponent, Localization);
		if (RequirementText.IsEmpty())
		{
			RequirementText = NSLOCTEXT("PandoraDescriptionWidget", "LockedRequirementFallback", "Unlock requirements are not met.");
		}

		ViewData.DescriptionText = RequirementText;
	}

	return ViewData;
}
