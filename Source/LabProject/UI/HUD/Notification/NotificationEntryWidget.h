#pragma once

#include "Common/NotificationData.h"
#include "UI/Common/LocalizedMenuWidget.h"
#include "NotificationEntryWidget.generated.h"

class UImage;
class UTextBlock;
class UWidgetAnimation;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UNotificationEntryWidget : public ULocalizedMenuWidget
{
	GENERATED_BODY()

protected:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativePreConstruct() override;

public:
	// Public API ------------------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "!UI|Notification")
	void SetNotificationData(const FNotificationData& InNotificationData);

	UFUNCTION(BlueprintCallable, Category = "!UI|Notification")
	void PlayNotificationIn();

	UFUNCTION(BlueprintCallable, Category = "!UI|Notification")
	float PlayNotificationOut();

protected:
	// Event Handlers --------------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintImplementableEvent, Category = "!UI|Notification", meta = (DisplayName = "On Notification Data Set"))
	void BP_OnNotificationDataSet(const FNotificationData& InNotificationData);

private:
	void ApplyNotificationData(const FNotificationData& InNotificationData, bool bNotifyBlueprint);
	static bool HasNotificationContent(const FNotificationData& InNotificationData);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!UI|Notification|Preview")
	FNotificationData PreviewNotificationData;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Notification|Bind")
	TObjectPtr<UImage> IconImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Notification|Bind")
	TObjectPtr<UTextBlock> NotificationText;

	UPROPERTY(Transient, BlueprintReadOnly, meta = (BindWidgetAnimOptional), Category = "!UI|Notification|Animation")
	TObjectPtr<UWidgetAnimation> FadeIn;

private:
	UPROPERTY(Transient)
	FNotificationData CachedNotificationData;

	UPROPERTY(Transient)
	bool bHasRuntimeNotificationData = false;
};
