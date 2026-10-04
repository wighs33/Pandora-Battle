#pragma once

#include "CoreMinimal.h"

class UButton;

// 버튼 하나와 그 클릭을 받을 UFUNCTION 이름. 이름은 GET_FUNCTION_NAME_CHECKED로 적어 함수가 없으면 컴파일이 멈추게 한다.
struct FPdButtonClickBinding
{
	UButton* Button = nullptr;
	FName HandlerName;
};

// 위젯이 만들어질 때 묶고 사라질 때 같은 표로 풀어, 버튼마다 두 번씩 적던 연결 코드를 표 하나로 줄인다.
// 위젯에 없는 버튼은 건너뛰고, 이미 묶인 처리기는 다시 묶지 않는다.
namespace PdButtonClick
{
	LABPROJECT_API void Bind(UObject* Handler, TConstArrayView<FPdButtonClickBinding> Bindings);
	LABPROJECT_API void Unbind(UObject* Handler, TConstArrayView<FPdButtonClickBinding> Bindings);
}
