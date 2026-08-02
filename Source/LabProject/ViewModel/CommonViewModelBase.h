#pragma once

#include "MVVMViewModelBase.h"
#include "CommonViewModelBase.generated.h"

UCLASS(Abstract)
class LABPROJECT_API UCommonViewModelBase : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UCommonViewModelBase();

	//-----------------------------------------------------------------------------
	// Timing hooks
	//-----------------------------------------------------------------------------

	virtual void InitializeViewModel(UObject* SourceObject = nullptr);

	virtual void UninitializeViewModel();

	bool IsViewModelInitialized() const { return bIsInitialized; }

protected:
	bool bIsInitialized = false;
};
