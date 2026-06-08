#pragma once

#include "Blueprint/UserWidget.h"
#include "UI/NotificationData.h"
#include "NotificationEntryWidget.generated.h"

class UImage;
class UTextBlock;
class UWidgetAnimation;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UNotificationEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "!UI|Notification")
	void SetNotificationData(const FPdNotificationData& InNotificationData);

	UFUNCTION(BlueprintCallable, Category = "!UI|Notification")
	void PlayNotificationIn();

	UFUNCTION(BlueprintCallable, Category = "!UI|Notification")
	float PlayNotificationOut();

protected:
	virtual void NativePreConstruct() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "!UI|Notification", meta = (DisplayName = "On Notification Data Set"))
	void BP_OnNotificationDataSet(const FPdNotificationData& InNotificationData);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!UI|Notification|Preview")
	FPdNotificationData PreviewNotificationData;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Notification|Bind")
	TObjectPtr<UImage> IconImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Notification|Bind")
	TObjectPtr<UTextBlock> NotificationText;

	UPROPERTY(Transient, BlueprintReadOnly, meta = (BindWidgetAnimOptional), Category = "!UI|Notification|Animation")
	TObjectPtr<UWidgetAnimation> FadeIn;

private:
	void CacheOptionalWidgets();
	void ApplyNotificationData(const FPdNotificationData& InNotificationData, bool bNotifyBlueprint);
	static bool HasNotificationContent(const FPdNotificationData& InNotificationData);

	UPROPERTY(Transient)
	FPdNotificationData CachedNotificationData;

	UPROPERTY(Transient)
	bool bHasRuntimeNotificationData = false;
};
