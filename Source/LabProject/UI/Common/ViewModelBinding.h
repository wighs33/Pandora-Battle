#pragma once

#include "CoreMinimal.h"

class UMVVMViewModelBase;
class UUserWidget;

namespace PdViewModelBinding
{
	/**
	 * 위젯 MVVM 뷰에서 ViewModelClass를 받을 수 있는 런타임 설정 소스 이름.
	 * PreferredName과 같은 소스가 있으면 그것을, 없으면 처음 맞는 소스를 고른다.
	 */
	FName FindSettableSourceName(const UUserWidget* Widget, const UClass* ViewModelClass, FName PreferredName = NAME_None);

	/** 맞는 소스에 ViewModel을 넣는다. MVVM 뷰가 없거나 받을 소스가 없으면 false. */
	bool SetViewModel(UUserWidget* Widget, UMVVMViewModelBase* ViewModel, FName PreferredName = NAME_None);
}
