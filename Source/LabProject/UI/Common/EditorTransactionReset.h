#pragma once

#include "CoreMinimal.h"

namespace PdEditorTransaction
{
	/** PIE에서 만든 UI 객체가 에디터 실행 취소 버퍼에 남아 있으면 버퍼를 비운다. 에디터가 아니면 아무 일도 하지 않는다. */
	LABPROJECT_API void ResetIfContainsPieObjects();
}
