#pragma once

#include "CoreMinimal.h"
#include "Components/SlateWrapperTypes.h"
#include "Styling/SlateColor.h"
#include "ViewModel/CommonViewModelBase.h"
#include "PandoraDescriptionViewModel.generated.h"

UCLASS(BlueprintType)
class LABPROJECT_API UPandoraDescriptionViewModel : public UCommonViewModelBase
{
	GENERATED_BODY()

public:
	UPandoraDescriptionViewModel();

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel")
	FText TitleText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel")
	FText DescriptionText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel")
	FText WeaponRequirementText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel")
	FSlateColor WeaponRequirementTextColor = FSlateColor(FLinearColor::White);

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel")
	ESlateVisibility WeaponRequirementVisibility = ESlateVisibility::Collapsed;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel")
	ESlateVisibility CurrentLevelVisibility = ESlateVisibility::Collapsed;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel")
	FText CurrentLevelTitleText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel")
	FText CurrentLevelDescriptionText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel")
	ESlateVisibility NextLevelVisibility = ESlateVisibility::Collapsed;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel")
	FText NextLevelTitleText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel")
	FText NextLevelDescriptionText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel")
	ESlateVisibility PointsRequiredVisibility = ESlateVisibility::Collapsed;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel")
	FText PointsRequiredText;

	void ResetViewData();
	void SetTitleText(const FText& InTitleText);
	void SetDescriptionText(const FText& InDescriptionText);
	void SetWeaponRequirementText(const FText& InWeaponRequirementText);
	void SetWeaponRequirementTextColor(const FSlateColor& InWeaponRequirementTextColor);
	void SetWeaponRequirementVisibility(ESlateVisibility InVisibility);
	void SetCurrentLevelVisibility(ESlateVisibility InVisibility);
	void SetCurrentLevelTitleText(const FText& InCurrentLevelTitleText);
	void SetCurrentLevelDescriptionText(const FText& InCurrentLevelDescriptionText);
	void SetNextLevelVisibility(ESlateVisibility InVisibility);
	void SetNextLevelTitleText(const FText& InNextLevelTitleText);
	void SetNextLevelDescriptionText(const FText& InNextLevelDescriptionText);
	void SetPointsRequiredVisibility(ESlateVisibility InVisibility);
	void SetPointsRequiredText(const FText& InPointsRequiredText);

	static const FName ViewModelName;
};
