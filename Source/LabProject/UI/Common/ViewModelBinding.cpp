#include "UI/Common/ViewModelBinding.h"

#include "Blueprint/UserWidget.h"
#include "MVVMViewModelBase.h"
#include "View/MVVMView.h"
#include "View/MVVMViewClass.h"

FName PdViewModelBinding::FindSettableSourceName(const UUserWidget* Widget, const UClass* ViewModelClass,
	const FName PreferredName)
{
	const UMVVMView* ViewExtension = Widget ? Widget->GetExtension<UMVVMView>() : nullptr;
	const UMVVMViewClass* ViewClass = ViewExtension ? ViewExtension->GetViewClass() : nullptr;
	if (!ViewClass || !ViewModelClass)
	{
		return NAME_None;
	}

	FName FirstCompatibleSourceName = NAME_None;
	for (const FMVVMViewClass_Source& Source : ViewClass->GetSources())
	{
		const UClass* SourceClass = Source.GetSourceClass();
		if (!Source.IsViewModel() || !Source.CanBeSet() || !SourceClass || !ViewModelClass->IsChildOf(SourceClass))
		{
			continue;
		}

		if (PreferredName.IsNone() || Source.GetName() == PreferredName)
		{
			return Source.GetName();
		}

		if (FirstCompatibleSourceName.IsNone())
		{
			FirstCompatibleSourceName = Source.GetName();
		}
	}

	return FirstCompatibleSourceName;
}

bool PdViewModelBinding::SetViewModel(UUserWidget* Widget, UMVVMViewModelBase* ViewModel, const FName PreferredName)
{
	UMVVMView* ViewExtension = Widget && ViewModel ? Widget->GetExtension<UMVVMView>() : nullptr;
	if (!ViewExtension)
	{
		return false;
	}

	const FName SourceName = FindSettableSourceName(Widget, ViewModel->GetClass(), PreferredName);
	return !SourceName.IsNone() && ViewExtension->SetViewModel(SourceName, ViewModel);
}
