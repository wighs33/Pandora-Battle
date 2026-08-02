#include "UI/Widget/KillLogWidget.h"

#include "Components/PanelWidget.h"
#include "TimerManager.h"
#include "UI/WidgetLookup.h"
#include "UI/Widget/KillLogEntryWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(KillLogWidget)

UKillLogWidget::UKillLogWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	KillLogContainerCandidateNames =
	{
		TEXT("VerticalBox_KillLogs"),
		TEXT("KillLogContainer"),
		TEXT("VB_KillLogs")
	};
}

void UKillLogWidget::NativeConstruct()
{
	Super::NativeConstruct();
	FindKillLogContainer();
}

void UKillLogWidget::AddKillLogEntry(const FKillLogEntry& KillLogEntry)
{
	UPanelWidget* Container = FindKillLogContainer();
	if (!Container)
	{

		return;
	}

	if (!KillLogEntryWidgetClass)
	{

		return;
	}

	UKillLogEntryWidget* EntryWidget = CreateWidget<UKillLogEntryWidget>(GetOwningPlayer(), KillLogEntryWidgetClass);
	if (!EntryWidget)
	{
		return;
	}

	EntryWidget->SetInfo(KillLogEntry);
	if (bNewestEntryOnTop)
	{
		Container->InsertChildAt(0, EntryWidget);
		ActiveEntries.Insert(EntryWidget, 0);
	}
	else
	{
		Container->AddChild(EntryWidget);
		ActiveEntries.Add(EntryWidget);
	}

	TrimOverflowEntries();

	if (EntryLifetime > 0.0f && GetWorld())
	{
		TWeakObjectPtr<UKillLogEntryWidget> WeakEntryWidget = EntryWidget;
		FTimerDelegate RemoveDelegate;
		RemoveDelegate.BindWeakLambda(this, [this, WeakEntryWidget]()
		{
			if (UKillLogEntryWidget* PinnedEntryWidget = WeakEntryWidget.Get())
			{
				RemoveKillLogEntry(PinnedEntryWidget);
			}
		});

		FTimerHandle TimerHandle;
		GetWorld()->GetTimerManager().SetTimer(TimerHandle, RemoveDelegate, EntryLifetime, false);
	}
}

UPanelWidget* UKillLogWidget::FindKillLogContainer()
{
	if (KillLogContainer)
	{
		return KillLogContainer;
	}

	if (!WidgetTree)
	{
		return nullptr;
	}

	if (UPanelWidget* FoundContainer =
		PdWidgetLookup::FindWidgetByNames<UPanelWidget>(WidgetTree, KillLogContainerCandidateNames))
	{
		KillLogContainer = FoundContainer;
		return FoundContainer;
	}

	UPanelWidget* FirstPanelWidget = PdWidgetLookup::FindFirstWidgetOfType<UPanelWidget>(WidgetTree);
	KillLogContainer = FirstPanelWidget;
	return FirstPanelWidget;
}

void UKillLogWidget::RemoveKillLogEntry(UKillLogEntryWidget* EntryWidget)
{
	if (!EntryWidget)
	{
		return;
	}

	ActiveEntries.Remove(EntryWidget);
	EntryWidget->RemoveFromParent();
}

void UKillLogWidget::TrimOverflowEntries()
{
	const int32 ClampedMaxVisibleEntries = FMath::Max(MaxVisibleEntries, 1);
	while (ActiveEntries.Num() > ClampedMaxVisibleEntries)
	{
		UKillLogEntryWidget* EntryToRemove = bNewestEntryOnTop ? ActiveEntries.Last() : ActiveEntries[0];
		RemoveKillLogEntry(EntryToRemove);
	}
}
