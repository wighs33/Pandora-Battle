#include "UI/Widget/RightNotificationsWidget.h"

#include "Components/PanelWidget.h"
#include "TimerManager.h"
#include "UI/Widget/NotificationEntryWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RightNotificationsWidget)

DEFINE_LOG_CATEGORY_STATIC(LogRightNotificationsWidget, Log, All);

void URightNotificationsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	CacheOptionalWidgets();
	UE_LOG(LogRightNotificationsWidget, Verbose,
		TEXT("[RightNotifications] Construct. widget=%s list=%s entryClass=%s maxVisible=%d lifetime=%.2f dequeueInterval=%.2f queued=%d available=%d active=%d"),
		*GetNameSafe(this),
		*GetNameSafe(NotificationList),
		*GetNameSafe(NotificationEntryWidgetClass.Get()),
		MaxVisibleNotifications,
		NotificationLifetime,
		NotificationDequeueInterval,
		NotificationQueue.Num(),
		AvailableNotifications.Num(),
		ActiveNotifications.Num());
	TryShowQueuedNotifications();
}

void URightNotificationsWidget::NativeDestruct()
{
	ClearNotifications();

	Super::NativeDestruct();
}

void URightNotificationsWidget::EnqueueNotification(const FPdNotificationData& NotificationData)
{
	if (NotificationData.Text.IsEmpty() && !NotificationData.IconResource)
	{
		UE_LOG(LogRightNotificationsWidget, Warning,
			TEXT("[RightNotifications] enqueue skipped: empty text and no icon. widget=%s"),
			*GetNameSafe(this));
		return;
	}

	NotificationQueue.Add(NotificationData);
	UE_LOG(LogRightNotificationsWidget, Verbose,
		TEXT("[RightNotifications] enqueued. widget=%s text=%s icon=%s queued=%d available=%d active=%d"),
		*GetNameSafe(this),
		*NotificationData.Text.ToString(),
		*GetNameSafe(NotificationData.IconResource),
		NotificationQueue.Num(),
		AvailableNotifications.Num(),
		ActiveNotifications.Num());
	TryShowQueuedNotifications();
}

void URightNotificationsWidget::ClearNotifications()
{
	UE_LOG(LogRightNotificationsWidget, Verbose,
		TEXT("[RightNotifications] ClearNotifications. widget=%s queued=%d available=%d active=%d lifetimeTimers=%d removeTimers=%d"),
		*GetNameSafe(this),
		NotificationQueue.Num(),
		AvailableNotifications.Num(),
		ActiveNotifications.Num(),
		LifetimeTimerHandles.Num(),
		RemoveTimerHandles.Num());

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DequeueTimerHandle);
	}
	bDequeueSequenceActive = false;

	ClearTimerMap(LifetimeTimerHandles);
	ClearTimerMap(RemoveTimerHandles);
	LifetimeTimerHandles.Empty();
	RemoveTimerHandles.Empty();
	NotificationQueue.Empty();

	for (UNotificationEntryWidget* ActiveNotification : ActiveNotifications)
	{
		if (ActiveNotification)
		{
			ActiveNotification->RemoveFromParent();
			ActiveNotification->SetVisibility(ESlateVisibility::Collapsed);
			AvailableNotifications.AddUnique(ActiveNotification);
		}
	}
	ActiveNotifications.Empty();
}

void URightNotificationsWidget::CacheOptionalWidgets()
{
	if (!NotificationList)
	{
		NotificationList = Cast<UPanelWidget>(GetWidgetFromName(TEXT("NotificationList")));
	}
	if (!NotificationList)
	{
		NotificationList = Cast<UPanelWidget>(GetWidgetFromName(TEXT("NotificationsList")));
	}
	if (!NotificationList)
	{
		NotificationList = Cast<UPanelWidget>(GetWidgetFromName(TEXT("NotificationsBox")));
	}
	if (!NotificationList)
	{
		NotificationList = Cast<UPanelWidget>(GetWidgetFromName(TEXT("VerticalBox_Notifications")));
	}

	if (!NotificationList)
	{
		UE_LOG(LogRightNotificationsWidget, Warning,
			TEXT("[RightNotifications] NotificationList not found. widget=%s expected one of: NotificationList, NotificationsList, NotificationsBox, VerticalBox_Notifications"),
			*GetNameSafe(this));
	}
}

void URightNotificationsWidget::PrimeNotificationPool()
{
	if (bNotificationPoolPrimed || !NotificationList)
	{
		return;
	}

	bNotificationPoolPrimed = true;

	for (int32 ChildIndex = NotificationList->GetChildrenCount() - 1; ChildIndex >= 0; --ChildIndex)
	{
		UNotificationEntryWidget* EntryWidget = Cast<UNotificationEntryWidget>(NotificationList->GetChildAt(ChildIndex));
		if (!EntryWidget)
		{
			continue;
		}

		NotificationList->RemoveChild(EntryWidget);
		EntryWidget->SetVisibility(ESlateVisibility::Collapsed);
		AvailableNotifications.AddUnique(EntryWidget);
	}

	UE_LOG(LogRightNotificationsWidget, Verbose,
		TEXT("[RightNotifications] pool primed. widget=%s list=%s available=%d"),
		*GetNameSafe(this),
		*GetNameSafe(NotificationList),
		AvailableNotifications.Num());
}

void URightNotificationsWidget::TryShowQueuedNotifications()
{
	CacheOptionalWidgets();
	PrimeNotificationPool();

	if (!NotificationList || (!NotificationEntryWidgetClass && AvailableNotifications.IsEmpty()))
	{
		UE_LOG(LogRightNotificationsWidget, Warning,
			TEXT("[RightNotifications] cannot start notification queue. widget=%s list=%s entryClass=%s queued=%d available=%d active=%d"),
			*GetNameSafe(this),
			*GetNameSafe(NotificationList),
			*GetNameSafe(NotificationEntryWidgetClass.Get()),
			NotificationQueue.Num(),
			AvailableNotifications.Num(),
			ActiveNotifications.Num());
		return;
	}

	if (bDequeueSequenceActive)
	{
		return;
	}

	bDequeueSequenceActive = true;
	ScheduleNextDequeue(0.0f);
}

bool URightNotificationsWidget::TryShowNextQueuedNotification()
{
	CacheOptionalWidgets();
	PrimeNotificationPool();

	if (!NotificationList || (!NotificationEntryWidgetClass && AvailableNotifications.IsEmpty()))
	{
		UE_LOG(LogRightNotificationsWidget, Warning,
			TEXT("[RightNotifications] cannot show next notification. widget=%s list=%s entryClass=%s queued=%d available=%d active=%d"),
			*GetNameSafe(this),
			*GetNameSafe(NotificationList),
			*GetNameSafe(NotificationEntryWidgetClass.Get()),
			NotificationQueue.Num(),
			AvailableNotifications.Num(),
			ActiveNotifications.Num());
		return false;
	}

	UE_LOG(LogRightNotificationsWidget, Verbose,
		TEXT("[RightNotifications] TryShowNextQueuedNotification. widget=%s queued=%d available=%d active=%d max=%d"),
		*GetNameSafe(this),
		NotificationQueue.Num(),
		AvailableNotifications.Num(),
		ActiveNotifications.Num(),
		MaxVisibleNotifications);

	if (NotificationQueue.IsEmpty())
	{
		return false;
	}

	UNotificationEntryWidget* EntryWidget = AcquireNotificationWidget();
	if (!EntryWidget)
	{
		UE_LOG(LogRightNotificationsWidget, Verbose,
			TEXT("[RightNotifications] wait for available widget. widget=%s queued=%d available=%d active=%d max=%d"),
			*GetNameSafe(this),
			NotificationQueue.Num(),
			AvailableNotifications.Num(),
			ActiveNotifications.Num(),
			MaxVisibleNotifications);
		return false;
	}

	const FPdNotificationData NotificationData = NotificationQueue[0];
	NotificationQueue.RemoveAt(0);

	UE_LOG(LogRightNotificationsWidget, Verbose,
		TEXT("[RightNotifications] showing notification. container=%s entry=%s text=%s icon=%s queuedAfter=%d available=%d activeBefore=%d"),
		*GetNameSafe(NotificationList),
		*GetNameSafe(EntryWidget),
		*NotificationData.Text.ToString(),
		*GetNameSafe(NotificationData.IconResource),
		NotificationQueue.Num(),
		AvailableNotifications.Num(),
		ActiveNotifications.Num());

	EntryWidget->SetNotificationData(NotificationData);
	EntryWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (EntryWidget->GetParent() != NotificationList)
	{
		NotificationList->AddChild(EntryWidget);
	}
	ActiveNotifications.AddUnique(EntryWidget);
	EntryWidget->PlayNotificationIn();
	ScheduleRemoveNotification(EntryWidget);
	return true;
}

void URightNotificationsWidget::ScheduleNextDequeue(float Delay)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FTimerManager& TimerManager = World->GetTimerManager();
	if (TimerManager.IsTimerActive(DequeueTimerHandle))
	{
		return;
	}

	UE_LOG(LogRightNotificationsWidget, Verbose,
		TEXT("[RightNotifications] schedule dequeue. widget=%s delay=%.2f queued=%d available=%d active=%d"),
		*GetNameSafe(this),
		Delay,
		NotificationQueue.Num(),
		AvailableNotifications.Num(),
		ActiveNotifications.Num());

	if (Delay <= 0.0f)
	{
		TimerManager.ClearTimer(DequeueTimerHandle);
		HandleDequeueTimer();
		return;
	}

	TimerManager.SetTimer(DequeueTimerHandle, this, &ThisClass::HandleDequeueTimer, Delay, false);
}

void URightNotificationsWidget::HandleDequeueTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DequeueTimerHandle);
	}

	const bool bShown = TryShowNextQueuedNotification();
	UE_LOG(LogRightNotificationsWidget, Verbose,
		TEXT("[RightNotifications] dequeue tick. widget=%s shown=%s queued=%d available=%d active=%d"),
		*GetNameSafe(this),
		bShown ? TEXT("true") : TEXT("false"),
		NotificationQueue.Num(),
		AvailableNotifications.Num(),
		ActiveNotifications.Num());

	if (!NotificationQueue.IsEmpty())
	{
		ScheduleNextDequeue(NotificationDequeueInterval);
		return;
	}

	if (bShown)
	{
		ScheduleNextDequeue(NotificationDequeueInterval);
		return;
	}

	bDequeueSequenceActive = false;
	UE_LOG(LogRightNotificationsWidget, Verbose,
		TEXT("[RightNotifications] dequeue sequence reset. widget=%s queued=%d available=%d active=%d"),
		*GetNameSafe(this),
		NotificationQueue.Num(),
		AvailableNotifications.Num(),
		ActiveNotifications.Num());
}

UNotificationEntryWidget* URightNotificationsWidget::AcquireNotificationWidget()
{
	if (ActiveNotifications.Num() >= MaxVisibleNotifications)
	{
		return nullptr;
	}

	while (!AvailableNotifications.IsEmpty())
	{
		UNotificationEntryWidget* EntryWidget = AvailableNotifications[0];
		AvailableNotifications.RemoveAt(0);
		if (EntryWidget)
		{
			return EntryWidget;
		}
	}

	APlayerController* OwningPlayer = GetOwningPlayer();
	if (!NotificationEntryWidgetClass)
	{
		UE_LOG(LogRightNotificationsWidget, Warning,
			TEXT("[RightNotifications] no available entry and NotificationEntryWidgetClass is not set. widget=%s owningPlayer=%s"),
			*GetNameSafe(this),
			*GetNameSafe(OwningPlayer));
		return nullptr;
	}

	UNotificationEntryWidget* EntryWidget = CreateWidget<UNotificationEntryWidget>(OwningPlayer, NotificationEntryWidgetClass);
	if (!EntryWidget)
	{
		UE_LOG(LogRightNotificationsWidget, Warning,
			TEXT("[RightNotifications] failed to create entry widget. widget=%s class=%s owningPlayer=%s"),
			*GetNameSafe(this),
			*GetNameSafe(NotificationEntryWidgetClass.Get()),
			*GetNameSafe(OwningPlayer));
		return nullptr;
	}

	EntryWidget->SetVisibility(ESlateVisibility::Collapsed);
	return EntryWidget;
}

void URightNotificationsWidget::ScheduleRemoveNotification(UNotificationEntryWidget* EntryWidget)
{
	if (!EntryWidget)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		const TWeakObjectPtr<UNotificationEntryWidget> EntryKey(EntryWidget);
		if (FTimerHandle* ExistingLifetimeHandle = LifetimeTimerHandles.Find(EntryKey))
		{
			World->GetTimerManager().ClearTimer(*ExistingLifetimeHandle);
			LifetimeTimerHandles.Remove(EntryKey);
		}

		FTimerHandle LifetimeTimerHandle;
		FTimerDelegate LifetimeDelegate = FTimerDelegate::CreateUObject(this, &ThisClass::BeginRemoveNotification, EntryWidget);
		World->GetTimerManager().SetTimer(LifetimeTimerHandle, LifetimeDelegate, NotificationLifetime, false);
		LifetimeTimerHandles.Add(EntryKey, LifetimeTimerHandle);
	}
}

void URightNotificationsWidget::BeginRemoveNotification(UNotificationEntryWidget* EntryWidget)
{
	if (!EntryWidget)
	{
		TryShowQueuedNotifications();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		const TWeakObjectPtr<UNotificationEntryWidget> EntryKey(EntryWidget);
		if (FTimerHandle* LifetimeHandle = LifetimeTimerHandles.Find(EntryKey))
		{
			World->GetTimerManager().ClearTimer(*LifetimeHandle);
			LifetimeTimerHandles.Remove(EntryKey);
		}
	}

	if (!ActiveNotifications.Contains(EntryWidget))
	{
		ReleaseNotificationWidget(EntryWidget);
		TryShowQueuedNotifications();
		return;
	}

	const float OutDuration = EntryWidget->PlayNotificationOut();
	UE_LOG(LogRightNotificationsWidget, Verbose,
		TEXT("[RightNotifications] begin remove notification. widget=%s entry=%s outDuration=%.2f active=%d queued=%d"),
		*GetNameSafe(this),
		*GetNameSafe(EntryWidget),
		OutDuration,
		ActiveNotifications.Num(),
		NotificationQueue.Num());

	if (OutDuration <= 0.0f)
	{
		FinishRemoveNotification(EntryWidget);
		return;
	}

	if (UWorld* World = GetWorld())
	{
		const TWeakObjectPtr<UNotificationEntryWidget> EntryKey(EntryWidget);
		FTimerHandle RemoveTimerHandle;
		FTimerDelegate RemoveDelegate = FTimerDelegate::CreateUObject(this, &ThisClass::FinishRemoveNotification, EntryWidget);
		World->GetTimerManager().SetTimer(RemoveTimerHandle, RemoveDelegate, OutDuration, false);
		RemoveTimerHandles.Add(EntryKey, RemoveTimerHandle);
	}
}

void URightNotificationsWidget::FinishRemoveNotification(UNotificationEntryWidget* EntryWidget)
{
	if (!EntryWidget)
	{
		TryShowQueuedNotifications();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		const TWeakObjectPtr<UNotificationEntryWidget> EntryKey(EntryWidget);
		if (FTimerHandle* RemoveHandle = RemoveTimerHandles.Find(EntryKey))
		{
			World->GetTimerManager().ClearTimer(*RemoveHandle);
			RemoveTimerHandles.Remove(EntryKey);
		}
	}

	UE_LOG(LogRightNotificationsWidget, Verbose,
		TEXT("[RightNotifications] finish remove notification. widget=%s entry=%s activeBefore=%d queued=%d"),
		*GetNameSafe(this),
		*GetNameSafe(EntryWidget),
		ActiveNotifications.Num(),
		NotificationQueue.Num());

	ActiveNotifications.Remove(EntryWidget);
	ReleaseNotificationWidget(EntryWidget);

	TryShowQueuedNotifications();
}

void URightNotificationsWidget::ReleaseNotificationWidget(UNotificationEntryWidget* EntryWidget)
{
	if (!EntryWidget)
	{
		return;
	}

	EntryWidget->RemoveFromParent();
	EntryWidget->SetVisibility(ESlateVisibility::Collapsed);
	AvailableNotifications.AddUnique(EntryWidget);
}

void URightNotificationsWidget::ClearTimerMap(TMap<TWeakObjectPtr<UNotificationEntryWidget>, FTimerHandle>& TimerMap)
{
	if (UWorld* World = GetWorld())
	{
		for (TPair<TWeakObjectPtr<UNotificationEntryWidget>, FTimerHandle>& Pair : TimerMap)
		{
			World->GetTimerManager().ClearTimer(Pair.Value);
		}
	}
}
