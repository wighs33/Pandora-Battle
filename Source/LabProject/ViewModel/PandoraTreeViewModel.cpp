#include "ViewModel/PandoraTreeViewModel.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PandoraTreeViewModel)

const FName UPandoraTreeViewModel::ViewModelName = TEXT("PandoraTreeViewModel");

UPandoraTreeViewModel::UPandoraTreeViewModel()
{
	ResetViewData();
}

void UPandoraTreeViewModel::ResetViewData()
{
	UE_MVVM_SET_PROPERTY_VALUE(PointsAvailable, 0);
	UE_MVVM_SET_PROPERTY_VALUE(PandoraPointsText, FText::GetEmpty());
}

void UPandoraTreeViewModel::SetPointsAvailable(int32 InPointsAvailable)
{
	UE_MVVM_SET_PROPERTY_VALUE(PointsAvailable, InPointsAvailable);
}

void UPandoraTreeViewModel::SetPandoraPointsText(const FText& InPandoraPointsText)
{
	UE_MVVM_SET_PROPERTY_VALUE(PandoraPointsText, InPandoraPointsText);
}
