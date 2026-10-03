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
	// Public API ------------------------------------------------------------------------------------------------------
	UPandoraDescriptionViewModel();

	void ResetViewData();
	void SetTitleText(const FText& InTitleText);
	void SetDescriptionText(const FText& InDescriptionText);
	void SetWeaponRequirementText(const FText& InWeaponRequirementText);
	void SetWeaponRequirementTextColor(const FSlateColor& InWeaponRequirementTextColor);
	void SetWeaponRequirementVisibility(ESlateVisibility InVisibility);
	void SetSkillSectionVisibility(ESlateVisibility InVisibility);
	void SetSkillSlot(int32 SlotIndex, UObject* InIconResource, const FText& InNameText, const FText& InDescriptionText, const FText& InManaText, const FText& InCooldownText);

public:
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

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel|Skills")
	ESlateVisibility SkillSectionVisibility = ESlateVisibility::Collapsed;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel|Skills")
	TObjectPtr<UObject> SkillIconResource1;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel|Skills")
	FText SkillNameText1;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel|Skills")
	FText SkillDescriptionText1;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel|Skills")
	FText SkillManaText1;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel|Skills")
	FText SkillCooldownText1;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel|Skills")
	ESlateVisibility SkillIconVisibility1 = ESlateVisibility::Collapsed;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel|Skills")
	TObjectPtr<UObject> SkillIconResource2;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel|Skills")
	FText SkillNameText2;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel|Skills")
	FText SkillDescriptionText2;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel|Skills")
	FText SkillManaText2;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel|Skills")
	FText SkillCooldownText2;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel|Skills")
	ESlateVisibility SkillIconVisibility2 = ESlateVisibility::Collapsed;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel|Skills")
	TObjectPtr<UObject> SkillIconResource3;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel|Skills")
	FText SkillNameText3;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel|Skills")
	FText SkillDescriptionText3;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel|Skills")
	FText SkillManaText3;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel|Skills")
	FText SkillCooldownText3;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel|Skills")
	ESlateVisibility SkillIconVisibility3 = ESlateVisibility::Collapsed;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel|Skills")
	TObjectPtr<UObject> SkillIconResource4;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel|Skills")
	FText SkillNameText4;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel|Skills")
	FText SkillDescriptionText4;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel|Skills")
	FText SkillManaText4;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel|Skills")
	FText SkillCooldownText4;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Description ViewModel|Skills")
	ESlateVisibility SkillIconVisibility4 = ESlateVisibility::Collapsed;

	static const FName ViewModelName;
};
