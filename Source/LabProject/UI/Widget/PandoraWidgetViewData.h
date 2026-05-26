#pragma once

#include "Components/SlateWrapperTypes.h"
#include "CoreMinimal.h"

class UPandoraDefinition;
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
	FText LevelText;
	UObject* IconResource = nullptr;
	FLinearColor OverlayColor = FLinearColor::Transparent;
	float ContentOpacity = 1.0f;
	ESlateVisibility StateIconVisibility = ESlateVisibility::Collapsed;
	FLinearColor StateIconColor = FLinearColor::White;
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

class LABPROJECT_API FPandoraWidgetViewDataBuilder
{
public:
	static FPandoraWidgetViewData Build(
		UPandoraDefinition* PandoraDefinition,
		const UPandoraTreeComponent* PandoraTreeComponent,
		const FPandoraWidgetStyleConfig& Style);

	static FText MakeLevelText(int32 CurrentLevel, int32 MaxLevel, bool bShowMaxText);
};
