#include "UI/HUD/Match/KillLogWidget.h"

#include "Components/VerticalBox.h"
#include "TimerManager.h"
#include "UI/HUD/Match/KillLogEntryWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(KillLogWidget)

void UKillLogWidget::AddKillLogEntry(const FKillLogEntry& KillLogEntry)
{
	if (!VerticalBox_KillLogs || !KillLogEntryWidgetClass)
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
		VerticalBox_KillLogs->InsertChildAt(0, EntryWidget);
		ActiveEntries.Insert(EntryWidget, 0);
	}
	else
	{
		VerticalBox_KillLogs->AddChild(EntryWidget);
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
