#pragma once

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "CoreMinimal.h"

namespace PdWidgetLookup
{
	template <typename WidgetType>
	WidgetType* FindWidgetByNames(const UWidgetTree* WidgetTree, const TArray<FName>& CandidateNames)
	{
		if (!WidgetTree)
		{
			return nullptr;
		}

		for (const FName& CandidateName : CandidateNames)
		{
			if (CandidateName.IsNone())
			{
				continue;
			}

			if (WidgetType* FoundWidget = Cast<WidgetType>(WidgetTree->FindWidget(CandidateName)))
			{
				return FoundWidget;
			}
		}

		return nullptr;
	}

	template <typename WidgetType>
	WidgetType* FindWidgetByNames(const UUserWidget* OwnerWidget, const TArray<FName>& CandidateNames)
	{
		if (!OwnerWidget)
		{
			return nullptr;
		}

		for (const FName& CandidateName : CandidateNames)
		{
			if (CandidateName.IsNone())
			{
				continue;
			}

			if (WidgetType* FoundWidget = Cast<WidgetType>(OwnerWidget->GetWidgetFromName(CandidateName)))
			{
				return FoundWidget;
			}
		}

		return nullptr;
	}

	template <typename WidgetType>
	WidgetType* FindFirstWidgetOfType(const UWidgetTree* WidgetTree)
	{
		if (!WidgetTree)
		{
			return nullptr;
		}

		WidgetType* Result = nullptr;
		WidgetTree->ForEachWidget([&Result](UWidget* Widget)
		{
			if (!Result)
			{
				Result = Cast<WidgetType>(Widget);
			}
		});

		return Result;
	}
}
