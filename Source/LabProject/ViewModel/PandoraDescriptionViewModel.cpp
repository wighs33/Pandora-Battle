#include "ViewModel/PandoraDescriptionViewModel.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PandoraDescriptionViewModel)

const FName UPandoraDescriptionViewModel::ViewModelName = TEXT("PandoraDescriptionViewModel");

UPandoraDescriptionViewModel::UPandoraDescriptionViewModel()
{
	ResetViewData();
}

void UPandoraDescriptionViewModel::ResetViewData()
{
	UE_MVVM_SET_PROPERTY_VALUE(TitleText, FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(DescriptionText, FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(WeaponRequirementText, FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(WeaponRequirementTextColor, FSlateColor(FLinearColor::White));
	UE_MVVM_SET_PROPERTY_VALUE(WeaponRequirementVisibility, ESlateVisibility::Collapsed);
	UE_MVVM_SET_PROPERTY_VALUE(CurrentLevelVisibility, ESlateVisibility::Collapsed);
	UE_MVVM_SET_PROPERTY_VALUE(CurrentLevelTitleText, FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(CurrentLevelDescriptionText, FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(NextLevelVisibility, ESlateVisibility::Collapsed);
	UE_MVVM_SET_PROPERTY_VALUE(NextLevelTitleText, FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(NextLevelDescriptionText, FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(PointsRequiredVisibility, ESlateVisibility::Collapsed);
	UE_MVVM_SET_PROPERTY_VALUE(PointsRequiredText, FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(SkillSectionVisibility, ESlateVisibility::Collapsed);
	SetSkillSlot(0, nullptr, FText::GetEmpty(), FText::GetEmpty());
	SetSkillSlot(1, nullptr, FText::GetEmpty(), FText::GetEmpty());
	SetSkillSlot(2, nullptr, FText::GetEmpty(), FText::GetEmpty());
	SetSkillSlot(3, nullptr, FText::GetEmpty(), FText::GetEmpty());
}

void UPandoraDescriptionViewModel::SetTitleText(const FText& InTitleText)
{
	UE_MVVM_SET_PROPERTY_VALUE(TitleText, InTitleText);
}

void UPandoraDescriptionViewModel::SetDescriptionText(const FText& InDescriptionText)
{
	UE_MVVM_SET_PROPERTY_VALUE(DescriptionText, InDescriptionText);
}

void UPandoraDescriptionViewModel::SetWeaponRequirementText(const FText& InWeaponRequirementText)
{
	UE_MVVM_SET_PROPERTY_VALUE(WeaponRequirementText, InWeaponRequirementText);
}

void UPandoraDescriptionViewModel::SetWeaponRequirementTextColor(const FSlateColor& InWeaponRequirementTextColor)
{
	UE_MVVM_SET_PROPERTY_VALUE(WeaponRequirementTextColor, InWeaponRequirementTextColor);
}

void UPandoraDescriptionViewModel::SetWeaponRequirementVisibility(ESlateVisibility InVisibility)
{
	UE_MVVM_SET_PROPERTY_VALUE(WeaponRequirementVisibility, InVisibility);
}

void UPandoraDescriptionViewModel::SetCurrentLevelVisibility(ESlateVisibility InVisibility)
{
	UE_MVVM_SET_PROPERTY_VALUE(CurrentLevelVisibility, InVisibility);
}

void UPandoraDescriptionViewModel::SetCurrentLevelTitleText(const FText& InCurrentLevelTitleText)
{
	UE_MVVM_SET_PROPERTY_VALUE(CurrentLevelTitleText, InCurrentLevelTitleText);
}

void UPandoraDescriptionViewModel::SetCurrentLevelDescriptionText(const FText& InCurrentLevelDescriptionText)
{
	UE_MVVM_SET_PROPERTY_VALUE(CurrentLevelDescriptionText, InCurrentLevelDescriptionText);
}

void UPandoraDescriptionViewModel::SetNextLevelVisibility(ESlateVisibility InVisibility)
{
	UE_MVVM_SET_PROPERTY_VALUE(NextLevelVisibility, InVisibility);
}

void UPandoraDescriptionViewModel::SetNextLevelTitleText(const FText& InNextLevelTitleText)
{
	UE_MVVM_SET_PROPERTY_VALUE(NextLevelTitleText, InNextLevelTitleText);
}

void UPandoraDescriptionViewModel::SetNextLevelDescriptionText(const FText& InNextLevelDescriptionText)
{
	UE_MVVM_SET_PROPERTY_VALUE(NextLevelDescriptionText, InNextLevelDescriptionText);
}

void UPandoraDescriptionViewModel::SetPointsRequiredVisibility(ESlateVisibility InVisibility)
{
	UE_MVVM_SET_PROPERTY_VALUE(PointsRequiredVisibility, InVisibility);
}

void UPandoraDescriptionViewModel::SetPointsRequiredText(const FText& InPointsRequiredText)
{
	UE_MVVM_SET_PROPERTY_VALUE(PointsRequiredText, InPointsRequiredText);
}

void UPandoraDescriptionViewModel::SetSkillSectionVisibility(const ESlateVisibility InVisibility)
{
	UE_MVVM_SET_PROPERTY_VALUE(SkillSectionVisibility, InVisibility);
}

void UPandoraDescriptionViewModel::SetSkillSlot(
	const int32 SlotIndex,
	UObject* InIconResource,
	const FText& InNameText,
	const FText& InDescriptionText)
{
	const ESlateVisibility IconVisibility = InIconResource ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed;

	switch (SlotIndex)
	{
	case 0:
		UE_MVVM_SET_PROPERTY_VALUE(SkillIconResource1, InIconResource);
		UE_MVVM_SET_PROPERTY_VALUE(SkillNameText1, InNameText);
		UE_MVVM_SET_PROPERTY_VALUE(SkillDescriptionText1, InDescriptionText);
		UE_MVVM_SET_PROPERTY_VALUE(SkillIconVisibility1, IconVisibility);
		break;
	case 1:
		UE_MVVM_SET_PROPERTY_VALUE(SkillIconResource2, InIconResource);
		UE_MVVM_SET_PROPERTY_VALUE(SkillNameText2, InNameText);
		UE_MVVM_SET_PROPERTY_VALUE(SkillDescriptionText2, InDescriptionText);
		UE_MVVM_SET_PROPERTY_VALUE(SkillIconVisibility2, IconVisibility);
		break;
	case 2:
		UE_MVVM_SET_PROPERTY_VALUE(SkillIconResource3, InIconResource);
		UE_MVVM_SET_PROPERTY_VALUE(SkillNameText3, InNameText);
		UE_MVVM_SET_PROPERTY_VALUE(SkillDescriptionText3, InDescriptionText);
		UE_MVVM_SET_PROPERTY_VALUE(SkillIconVisibility3, IconVisibility);
		break;
	case 3:
		UE_MVVM_SET_PROPERTY_VALUE(SkillIconResource4, InIconResource);
		UE_MVVM_SET_PROPERTY_VALUE(SkillNameText4, InNameText);
		UE_MVVM_SET_PROPERTY_VALUE(SkillDescriptionText4, InDescriptionText);
		UE_MVVM_SET_PROPERTY_VALUE(SkillIconVisibility4, IconVisibility);
		break;
	default:
		break;
	}
}
