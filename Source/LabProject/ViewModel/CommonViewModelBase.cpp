#include "CommonViewModelBase.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(CommonViewModelBase)

UCommonViewModelBase::UCommonViewModelBase()
{
}

void UCommonViewModelBase::InitializeViewModel(UObject* SourceObject)
{
	bIsInitialized = true;
}

void UCommonViewModelBase::UninitializeViewModel()
{
	bIsInitialized = false;
}
