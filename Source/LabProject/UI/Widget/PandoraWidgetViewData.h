#pragma once

#include "Components/SlateWrapperTypes.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

class UUserWidget;
class UPandoraDefinition;
class UPandoraInstance;
class UPandoraTreeComponent;

struct LABPROJECT_API FPandoraWidgetStyleConfig
{
	FLinearColor AvailableOverlayColor = FLinearColor::Transparent;
	FLinearColor UnavailableOverlayColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.55f);
	FLinearColor LockedOverlayColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.65f);
	FLinearColor NotEnoughPointsOverlayColor = FLinearColor(0.75f, 0.0f, 0.0f, 0.35f);
	float AvailableContentOpacity = 1.0f;
	float UnavailableContentOpacity = 0.35f;
	bool bShowMaxText = true;
};

struct LABPROJECT_API FPandoraWidgetViewData
{
	FText DisplayName;
	FText Description;
	FText LevelText;
	UObject* IconResource = nullptr;
	FGameplayTagContainer RequiredWeaponTags;
	FLinearColor OverlayColor = FLinearColor::Transparent;
	float ContentOpacity = 1.0f;
	ESlateVisibility StateIconVisibility = ESlateVisibility::Collapsed;
	FLinearColor StateIconColor = FLinearColor::White;
	bool bOwned = false;
	bool bActive = false;
	bool bCanSpend = false;
	bool bLocked = false;
	bool bNotEnoughPoints = false;
	bool bAtMaxLevel = false;
	bool bInactiveStyle = false;
	bool bDimmedStyle = false;
	bool bUnlockRulesMet = true;
	int32 CurrentLevel = 0;
	int32 MaxLevel = 0;
	int32 PointsAvailable = INDEX_NONE;
	int32 RequiredPoints = INDEX_NONE;
};

struct LABPROJECT_API FPandoraSlotViewData
{
	FText DisplayName;
	FText Description;
	UObject* IconResource = nullptr;
	FGameplayTagContainer RequiredWeaponTags;
	bool bOwned = false;
	bool bActive = false;
	bool bEnabled = false;
};

struct LABPROJECT_API FPandoraSkillSlotViewData
{
	UObject* IconResource = nullptr;
	FText DisplayName;
	FText Description;
};

struct LABPROJECT_API FPandoraDescriptionViewData
{
	FText TitleText;
	FText DescriptionText;
	FText WeaponRequirementText;
	ESlateVisibility WeaponRequirementVisibility = ESlateVisibility::Collapsed;
	ESlateVisibility CurrentLevelVisibility = ESlateVisibility::Collapsed;
	FText CurrentLevelTitleText;
	FText CurrentLevelDescriptionText;
	ESlateVisibility NextLevelVisibility = ESlateVisibility::Collapsed;
	FText NextLevelTitleText;
	FText NextLevelDescriptionText;
	ESlateVisibility PointsRequiredVisibility = ESlateVisibility::Collapsed;
	FText PointsRequiredText;
	ESlateVisibility SkillSectionVisibility = ESlateVisibility::Collapsed;
	TArray<FPandoraSkillSlotViewData> SkillSlots;
	bool bHasPandoraDefinition = false;
	bool bLockedByPandoraRequirement = false;
	int32 CurrentLevel = 0;
	int32 NextLevel = 1;
	int32 MaxLevel = 1;
};

class LABPROJECT_API FPandoraWidgetViewDataBuilder
{
public:
	static FPandoraWidgetViewData Build(
		UPandoraDefinition* PandoraDefinition,
		const UPandoraTreeComponent* PandoraTreeComponent,
		const FPandoraWidgetStyleConfig& Style);

	static UPandoraDefinition* GetSelectedPandoraDefinition(const UPandoraTreeComponent* PandoraTreeComponent);
	static bool IsPandoraOwnedInProfile(const UUserWidget* Widget, UPandoraDefinition* PandoraDefinition);

	static FText MakeLevelText(int32 CurrentLevel, int32 MaxLevel, bool bShowMaxText);
};

class LABPROJECT_API FPandoraSlotViewDataBuilder
{
public:
	static FPandoraSlotViewData Build(const UPandoraInstance* PandoraInstance);
};

class LABPROJECT_API FPandoraDescriptionViewDataBuilder
{
public:
	static FPandoraDescriptionViewData Build(
		UPandoraDefinition* PandoraDefinition,
		const UPandoraTreeComponent* PandoraTreeComponent);
};
