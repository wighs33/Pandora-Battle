#include "UI/Widget/RightNotificationsWidget.h"

#include "Components/PanelWidget.h"
#include "TimerManager.h"
#include "UI/Widget/NotificationEntryWidget.h"
#include "UI/WidgetLookup.h"
#include "Definition/UI/WidgetClassDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RightNotificationsWidget)

void URightNotificationsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ApplyWidgetDefinitionSettings();
	CacheOptionalWidgets();

	TryShowQueuedNotifications();
}

void URightNotificationsWidget::NativeDestruct()
{
	ClearNotifications();

	Super::NativeDestruct();
}

void URightNotificationsWidget::ApplyWidgetDefinitionSettings()
{
	if (const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this))
	{
		const FRightNotificationsWidgetSettings& Settings = WidgetDefinition->GetRightNotificationsWidgetSettings();
		if (const TSubclassOf<UNotificationEntryWidget> ResolvedEntryWidgetClass =
			WidgetDefinition->GetNotificationEntryWidgetClass())
		{
			NotificationEntryWidgetClass = ResolvedEntryWidgetClass;
		}
		MaxVisibleNotifications = FMath::Max(Settings.MaxVisibleNotifications, 1);
		NotificationLifetime = FMath::Max(Settings.NotificationLifetime, 0.0f);
		NotificationDequeueInterval = FMath::Max(Settings.NotificationDequeueInterval, 0.0f);
	}
}

void URightNotificationsWidget::EnqueueNotification(const FPdNotificationData& NotificationData)
{
	if (NotificationData.Text.IsEmpty() && !NotificationData.IconResource)
	{

		return;
	}

	NotificationQueue.Add(NotificationData);

	TryShowQueuedNotifications();
}

void URightNotificationsWidget::ClearNotifications()
{

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
		NotificationList = PdWidgetLookup::FindWidgetByNames<UPanelWidget>(this, {
			TEXT("NotificationList"),
			TEXT("NotificationsList"),
			TEXT("NotificationsBox"),
			TEXT("VerticalBox_Notifications")
		});
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

}

void URightNotificationsWidget::TryShowQueuedNotifications()
{
	CacheOptionalWidgets();
	PrimeNotificationPool();

	if (!NotificationList || (!NotificationEntryWidgetClass && AvailableNotifications.IsEmpty()))
	{

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

		return false;
	}

	if (NotificationQueue.IsEmpty())
	{
		return false;
	}

	UNotificationEntryWidget* EntryWidget = AcquireNotificationWidget();
	if (!EntryWidget)
	{

		return false;
	}

	const FPdNotificationData NotificationData = NotificationQueue[0];
	NotificationQueue.RemoveAt(0);

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

		return nullptr;
	}

	UNotificationEntryWidget* EntryWidget = CreateWidget<UNotificationEntryWidget>(OwningPlayer, NotificationEntryWidgetClass);
	if (!EntryWidget)
	{

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
