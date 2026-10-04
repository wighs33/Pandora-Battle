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

	namespace Detail
	{
		template <typename WidgetType, typename FunctorType>
		void VisitNestedWidgetsOfType(const UUserWidget* OwnerWidget, FunctorType& Visit)
		{
			if (!OwnerWidget || !OwnerWidget->WidgetTree)
			{
				return;
			}

			// ForEachWidget은 안에 든 UUserWidget 자체는 방문하지만 그 위젯의 WidgetTree 안으로는 들어가지 않으므로 직접 재귀한다.
			OwnerWidget->WidgetTree->ForEachWidget([&Visit](UWidget* Widget)
			{
				if (WidgetType* TypedWidget = Cast<WidgetType>(Widget))
				{
					Visit(TypedWidget);
				}
				if (const UUserWidget* ChildUserWidget = Cast<UUserWidget>(Widget))
				{
					VisitNestedWidgetsOfType<WidgetType>(ChildUserWidget, Visit);
				}
			});
		}
	}

	// RootWidget 자신과 그 트리, 트리 안 사용자 위젯의 트리까지 내려가며 만나는 WidgetType마다 Visit을 부른다.
	template <typename WidgetType, typename FunctorType>
	void ForEachNestedWidgetOfType(UUserWidget* RootWidget, FunctorType Visit)
	{
		if (WidgetType* TypedRoot = Cast<WidgetType>(RootWidget))
		{
			Visit(TypedRoot);
		}
		Detail::VisitNestedWidgetsOfType<WidgetType>(RootWidget, Visit);
	}
}
