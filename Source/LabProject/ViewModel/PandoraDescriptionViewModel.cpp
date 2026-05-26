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
