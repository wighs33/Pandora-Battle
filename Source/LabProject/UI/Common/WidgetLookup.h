#pragma once

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "CoreMinimal.h"

namespace PdWidgetLookup
{
	// OwnerWidget의 위젯 트리에서 처음 만나는 WidgetType을 찾는다. 트리에 종류별로 하나만 배치된 위젯에만 쓴다.
	template <typename WidgetType>
	WidgetType* FindFirstWidgetOfType(const UUserWidget* OwnerWidget)
	{
		if (!OwnerWidget || !OwnerWidget->WidgetTree)
		{
			return nullptr;
		}

		WidgetType* Result = nullptr;
		OwnerWidget->WidgetTree->ForEachWidget([&Result](UWidget* Widget)
		{
			if (!Result)
			{
				Result = Cast<WidgetType>(Widget);
			}
		});

		return Result;
	}
}
