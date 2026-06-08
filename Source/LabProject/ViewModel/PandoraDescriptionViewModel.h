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

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel|Skills")
	ESlateVisibility SkillSectionVisibility = ESlateVisibility::Collapsed;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel|Skills")
	TObjectPtr<UObject> SkillIconResource1;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel|Skills")
	FText SkillNameText1;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel|Skills")
	FText SkillDescriptionText1;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel|Skills")
	ESlateVisibility SkillIconVisibility1 = ESlateVisibility::Collapsed;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel|Skills")
	TObjectPtr<UObject> SkillIconResource2;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel|Skills")
	FText SkillNameText2;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel|Skills")
	FText SkillDescriptionText2;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel|Skills")
	ESlateVisibility SkillIconVisibility2 = ESlateVisibility::Collapsed;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel|Skills")
	TObjectPtr<UObject> SkillIconResource3;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel|Skills")
	FText SkillNameText3;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel|Skills")
	FText SkillDescriptionText3;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel|Skills")
	ESlateVisibility SkillIconVisibility3 = ESlateVisibility::Collapsed;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel|Skills")
	TObjectPtr<UObject> SkillIconResource4;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel|Skills")
	FText SkillNameText4;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel|Skills")
	FText SkillDescriptionText4;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel|Skills")
	ESlateVisibility SkillIconVisibility4 = ESlateVisibility::Collapsed;

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
	void SetSkillSectionVisibility(ESlateVisibility InVisibility);
	void SetSkillSlot(int32 SlotIndex, UObject* InIconResource, const FText& InNameText, const FText& InDescriptionText);

	static const FName ViewModelName;
};
