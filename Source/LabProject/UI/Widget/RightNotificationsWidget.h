#pragma once

#include "Blueprint/UserWidget.h"
#include "UI/NotificationData.h"
#include "RightNotificationsWidget.generated.h"

class UNotificationEntryWidget;
class UPanelWidget;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API URightNotificationsWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

public:
	// Public API ------------------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "!UI|Notification")
	void EnqueueNotification(const FPdNotificationData& NotificationData);

	UFUNCTION(BlueprintCallable, Category = "!UI|Notification")
	void ClearNotifications();

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	void HandleDequeueTimer();
	void BeginRemoveNotification(UNotificationEntryWidget* EntryWidget);
	void FinishRemoveNotification(UNotificationEntryWidget* EntryWidget);

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void ApplyWidgetDefinitionSettings();
	void CacheOptionalWidgets();
	void PrimeNotificationPool();
	void TryShowQueuedNotifications();
	bool TryShowNextQueuedNotification();
	void ScheduleNextDequeue(float Delay);
	UNotificationEntryWidget* AcquireNotificationWidget();
	void ScheduleRemoveNotification(UNotificationEntryWidget* EntryWidget);
	void ReleaseNotificationWidget(UNotificationEntryWidget* EntryWidget);
	void ClearTimerMap(TMap<TWeakObjectPtr<UNotificationEntryWidget>, FTimerHandle>& TimerMap);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!UI|Notification")
	TSubclassOf<UNotificationEntryWidget> NotificationEntryWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!UI|Notification", meta = (ClampMin = "1"))
	int32 MaxVisibleNotifications = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!UI|Notification", meta = (ClampMin = "0.0"))
	float NotificationLifetime = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!UI|Notification", meta = (ClampMin = "0.0"))
	float NotificationDequeueInterval = 0.5f;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Notification|Bind")
	TObjectPtr<UPanelWidget> NotificationList;

private:
	UPROPERTY(Transient)
	TArray<FPdNotificationData> NotificationQueue;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UNotificationEntryWidget>> AvailableNotifications;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UNotificationEntryWidget>> ActiveNotifications;

	TMap<TWeakObjectPtr<UNotificationEntryWidget>, FTimerHandle> LifetimeTimerHandles;
	TMap<TWeakObjectPtr<UNotificationEntryWidget>, FTimerHandle> RemoveTimerHandles;
	FTimerHandle DequeueTimerHandle;
	bool bNotificationPoolPrimed = false;
	bool bDequeueSequenceActive = false;
};
