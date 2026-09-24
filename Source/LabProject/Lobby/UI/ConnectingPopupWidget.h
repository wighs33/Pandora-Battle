#pragma once

#include "CoreMinimal.h"
#include "UI/Widget/LocalizedMenuWidget.h"
#include "ConnectingPopupWidget.generated.h"

class UButton;
class UWidgetAnimation;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FConnectingPopupCanceledSignature);

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UConnectingPopupWidget : public ULocalizedMenuWidget
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// Public API ------------------------------------------------------------------------------------------------------
	UConnectingPopupWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "!Lobby|Connecting")
	void SetCancelButtonEnabled(bool bEnabled);

protected:
	// Event Handlers --------------------------------------------------------------------------------------------------
	UFUNCTION()
	void HandleCancelClicked();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void ApplyCancelButtonState() const;
	void PlayWaitAnimation();
	void StopWaitAnimation();

public:
	UPROPERTY(BlueprintAssignable, Category = "!Lobby|Connecting")
	FConnectingPopupCanceledSignature OnCanceled;

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UButton> Btn_Cancel;

	UPROPERTY(Transient, BlueprintReadOnly, meta = (BindWidgetAnimOptional), Category = "!Lobby|Animation")
	TObjectPtr<UWidgetAnimation> WaitAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Lobby|Connecting")
	bool bCancelButtonEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Lobby|Animation", meta = (ClampMin = "0.01"))
	float WaitAnimationPlaybackSpeed = 1.0f;
};
